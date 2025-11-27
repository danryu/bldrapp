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

## Notes

- Audio device selection is disabled while connected (same as camera)
- Empty `audioDeviceIndex` means "use system default microphone"
- CoreAudio device IDs are stable across reboots but may change when devices are unplugged/replugged

