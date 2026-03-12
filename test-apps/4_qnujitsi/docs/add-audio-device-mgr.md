# Add Audio Device Manager

This document describes the changes made to add microphone input support with device selection to qnujitsi.

## Overview

Previously, qnujitsi used `audiotestsrc` to generate synthetic test audio. This change replaces that with real microphone capture via `osxaudiosrc` (macOS CoreAudio) and adds a UI dropdown for selecting the audio input device.

## Changes

### 1. Replace Test Audio with Microphone Capture

**Files:** `send_pipeline.h`, `send_pipeline.cpp`

Changed the audio pipeline from:
```
audiotestsrc → opusenc → jitsibin
```
To:
```
osxaudiosrc → audioconvert → opusenc → jitsibin
```

- Renamed `audiotestsrc_` member to `audiosrc_`
- Added `audioconvert_` element for format conversion (osxaudiosrc may output various formats)
- Added `audioDeviceIndex` parameter to `buildAndLink()` and `configureElements()`
- Configure osxaudiosrc device via `g_object_set(audiosrc_, "device", audioDevice, NULL)`

### 2. Create AudioManager Class

**Files:** `audio_manager.h`, `audio_manager.cpp`

New class mirroring `CameraManager` for audio device enumeration:

- Uses `GstDeviceMonitor` with `Audio/Source` filter
- Filters for `osxaudiosrc` factory name (macOS-specific)
- Exposes QML properties: `audioDeviceNames`, `currentAudioDeviceIndex`
- Provides `selectedAudioDevice()` for retrieving the selected device info

### 3. Integrate AudioManager into AppController

**Files:** `app_controller.h`, `app_controller.cpp`

- Added `AudioManager` member with Q_PROPERTY
- Initialize and enumerate audio devices on startup
- Pass selected audio device index to conference when connecting

### 4. Add UI Dropdown

**File:** `main.qml`

Added "Mic:" dropdown in toolbar:
```qml
Label { text: "Mic:"; color: "white" }
ComboBox {
    id: audioCombo
    model: AppController.audioManager.audioDeviceNames
    currentIndex: AppController.audioManager.currentAudioDeviceIndex
    enabled: !AppController.connected
    onCurrentIndexChanged: {
        if (currentIndex !== AppController.audioManager.currentAudioDeviceIndex) {
            AppController.audioManager.currentAudioDeviceIndex = currentIndex
        }
    }
    Layout.preferredWidth: 140
}
```

### 5. Update Conference to Pass Audio Device

**Files:** `conference.h`, `conference.cpp`

Added `audioDeviceIndex` parameter to `initQmlSlotsAndSend()` and pass through to `SendPipeline::buildAndLink()`.

### 6. Build System

**File:** `CMakeLists.txt`

Added `audio_manager.cpp` to SOURCES.

## Bug Fix: CoreAudio Device ID Handling

### Problem

Initial implementation used sequential indices (0, 1, 2...) when enumerating audio devices. However, `osxaudiosrc`'s `device` property expects the **actual CoreAudio AudioDeviceID**, which is an arbitrary integer assigned by the system (e.g., 39, 57, 112).

This caused USB audio interfaces (like Behringer UPhoria UMC204HD) to fail with:
```
CoreAudio device not found
```

### Root Cause

When enumerating, we stored:
```cpp
audioDevices_.emplace_back(std::to_string(deviceIndex), displayName);
deviceIndex++;  // Wrong! Sequential index, not CoreAudio ID
```

### Solution

Read the actual CoreAudio device ID from the element created by `gst_device_create_element()`:
```cpp
GstElement* element = gst_device_create_element(device, nullptr);
// ...
gint coreAudioDeviceId = 0;
g_object_get(G_OBJECT(element), "device", &coreAudioDeviceId, NULL);
audioDevices_.emplace_back(std::to_string(coreAudioDeviceId), displayName);
```

Also removed the fallback sequential enumeration (it can't work for CoreAudio) and replaced with a simple "use system default" fallback (empty device name = no device property set).

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│ AppController                                                    │
│   ├── CameraManager (enumerates avfvideosrc devices)            │
│   ├── AudioManager (enumerates osxaudiosrc devices)             │
│   └── Conference                                                 │
│         └── SendPipeline                                         │
│               ├── Video: avfvideosrc/videotestsrc → ... → jitsibin │
│               └── Audio: osxaudiosrc → audioconvert → opusenc → jitsibin │
└─────────────────────────────────────────────────────────────────┘
```

## Key GStreamer Elements

| Element | Purpose |
|---------|---------|
| `osxaudiosrc` | macOS CoreAudio input capture |
| `audioconvert` | Audio format conversion |
| `opusenc` | Opus audio encoding |
| `avfvideosrc` | macOS AVFoundation video capture |

## Audio Receive Handling

### Problem 1: Unhandled Audio Pads

After enabling audio send, incoming Opus audio pads from jitsibin were not handled. The code assumed all pads were H.264 video, causing:
```
Incoming pad f1c70e58_OPUS_2667144609: assuming raw H.264
Failed to link jitsibin pad -> h264parse
```

### Solution

Updated `ParticipantManager` to detect codec from pad name and route to appropriate handler:

1. **getCodecFromPadName()** - Parses codec from pad name (format: `participantId_CODEC_ssrc`)
2. **handlePadAdded()** - Routes to `handleAudioPadAdded()` for OPUS, `handleVideoPadAdded()` for H264/VP8/VP9
3. **handleAudioPadAdded()** - Creates audio receive chain
4. **AudioSession struct** - Tracks audio chain elements for cleanup
5. **teardownAudioPad()** - Cleans up audio sessions when participant leaves

### Problem 2: Video Frame Rate Drop When Audio Added Dynamically

**Symptom:** When a remote participant unmuted audio *after* qnujitsi had already joined:
- Video frame rate dropped from 30fps to ~5-15fps
- Video latency increased to ~800ms
- No issue if participant was already unmuted when qnujitsi joined

**Root Cause:** Adding audio sink to a running PLAYING pipeline caused clock/timing disruption:
1. `autoaudiosink` is a wrapper bin - state transitions affected the whole pipeline
2. `async` property wasn't being applied (`autoaudiosink` doesn't support it directly)
3. State syncing was done sink-first (wrong order for hot-adding elements)
4. Linking to jitsibin pad before elements were ready caused data to flow into unprepared chain

### Solution

Complete rewrite of `handleAudioPadAdded()`:

1. **Use `osxaudiosink` directly** instead of `autoaudiosink`
   - Direct control over `sync` and `async` properties
   - No lazy sink creation during state transitions

2. **Add a queue before the sink** for decoupling
   - Prevents audio processing from blocking video pipeline
   - Leaky downstream to drop old buffers if queue fills

3. **Correct state sync order** (upstream → downstream)
   ```cpp
   gst_element_sync_state_with_parent(dec);      // First
   gst_element_sync_state_with_parent(conv);
   gst_element_sync_state_with_parent(resample);
   gst_element_sync_state_with_parent(queue);
   gst_element_sync_state_with_parent(sink);     // Last
   ```

4. **Link chain before connecting to jitsibin**
   - Build and sync the entire audio chain first
   - Only then connect the source pad
   - Data flows into an already-running chain

### Final Audio Receive Chain

```
jitsibin pad → opusdec → audioconvert → audioresample → queue → osxaudiosink
                                                          ↑
                                               (decoupling buffer,
                                                leaky downstream)
```

### Audio Sink Configuration

```cpp
g_object_set(G_OBJECT(sink),
             "sync", FALSE,   // Play immediately without clock sync
             "async", FALSE,  // Don't affect pipeline state transitions
             NULL);

g_object_set(G_OBJECT(queue),
             "max-size-buffers", 2,
             "leaky", 2,      // GST_QUEUE_LEAK_DOWNSTREAM
             NULL);
```

## Notes

- Audio device selection is disabled while connected (same as camera)
- Empty `audioDeviceIndex` means "use system default microphone"
- CoreAudio device IDs are stable across reboots but may change when devices are unplugged/replugged
- Each participant can have both video and audio pads (tracked separately)
- Audio receive uses `sync=FALSE` to prevent clock interference with video

