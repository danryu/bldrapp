## qnujitsi

qnujitsi is a minimal Qt/QML application that demonstrates real-time audio/video send/receive into a Jitsi Meet conference using the custom `jitsibin` GStreamer element (from gstjitsimeet). It renders video via `qml6glsink` using Qt Quick's OpenGL backend and publishes local test AV.

## Architecture

### Components

- **AppController** - Qt singleton managing conference lifecycle and QML integration
- **Conference** - Orchestrates pipeline, jitsibin, send, and participant management
- **SendPipeline** - Encapsulates local AV send path with tee for local preview
- **ParticipantManager** - Manages receive slots and dynamic pad handling for remote participants

### Pipeline Overview

```
┌─────────────────── Send Path ───────────────────┐
│ videotestsrc → videoscale → tee                 │
│   ├─ queue → videoconvert (slot 0) → [local]    │
│   └─ vtenc_h264 → h264parse → queue → jitsibin  │
│                                                  │
│ audiotestsrc → opusenc → jitsibin               │
└──────────────────────────────────────────────────┘

┌─────────────────── Receive Path (per participant) ───────────────────┐
│ jitsibin:src_* → h264parse → vtdec → videoconvert → queue →          │
│                  glupload → glcolorconvert → qml6glsink (slot 1-3)   │
└───────────────────────────────────────────────────────────────────────┘
```

### Slot Allocation

- **Slot 0:** Local preview (reserved at initialization)
- **Slots 1-3:** Remote participants (dynamically allocated via jitsibin pad-added signal)

QML declares 4 `GstGLQt6VideoItem` elements (`videoItem0` - `videoItem3`) in a 2x2 grid. C++ discovers these by `objectName` and pre-creates receive chains (videoconvert → queue → glupload → glcolorconvert → qml6glsink) for each slot.

### Initialization Sequence

1. **Conference::build()** - Creates pipeline, jitsibin, and ParticipantManager; configures jitsibin properties
2. **Conference::initQmlSlotsAndSend()** - Initializes QML slots, reserves slot 0, builds SendPipeline with local preview tee
3. **Conference::scheduleStart()** - Sets pipeline to PLAYING on Qt render thread

### Dynamic Participant Handling

When jitsibin emits `pad-added` signal with new remote participant stream:

1. ParticipantManager::handlePadAdded() acquires free slot (1-3) under mutex
2. Creates receive chain: h264parse → vtdec → existing slot videoconvert
3. Links jitsibin src pad to h264parse
4. Syncs element states with parent pipeline
5. Marks slot as in-use

### Thread Safety

- **Slot allocation:** Protected by `slotMutex_` to prevent races during concurrent pad-added callbacks
- **Pipeline state changes:** Scheduled on Qt render thread via `QQuickWindow::scheduleRenderJob()`
- **QML updates:** Automatic via GStreamer's qml6glsink Qt integration

### Key GStreamer Elements

- **jitsibin** - Custom element handling Jitsi XMPP/Colibri/WebRTC signaling and RTP transport
- **vtenc_h264** - Hardware H.264 encoder (VideoToolbox on macOS)
- **vtdec** - Hardware H.264 decoder with automatic sync point recovery
- **qml6glsink** - Qt6 QML video sink with OpenGL texture sharing
- **tee** - Splits raw video to local preview and encoder

### Configuration

Jitsibin properties set in Conference::build():
- `server` - Jitsi server hostname
- `room` - Conference room name
- `nick` - Participant display name
- `video-codec` - 1 (H264)
- `receive-limit` - Max remote participants (4)
- `receive-max-height` - Max receive resolution (720p)
- `jitterbuffer-latency` - RTP jitter buffer (300ms)
- `force-play` - Auto-start on connection
- `insecure` - Allow self-signed certificates

### Low-Latency Optimizations

- **Encoder queue:** Leaky downstream, unbounded size (immediate encoding)
- **Local preview queue:** Leaky downstream, 5 buffer limit (drop old frames)
- **Receive queue:** Leaky downstream, 5 buffer limit (smooth GL delivery)
- **vtenc_h264:** `realtime=TRUE`, `allow-frame-reordering=FALSE`
- **h264parse:** `config-interval=1` (SPS/PPS with every IDR)
- **vtdec:** `automatic-request-sync-points=TRUE` with corrupt output flag
- **qml6glsink:** `sync=TRUE`, `async=FALSE` (frame timing without artifacts)

### Qt/GStreamer Integration

Static Qt build with embedded resources. CMake finds:
- GStreamer via `gstreamer-full-1.0.pc` (static monolithic library)
- Qt6 via custom prefix (`/Users/dan/qt6-static-build`)
- Explicitly links `libgstqml6.a` for qml6glsink plugin

Resource file `qnujitsi.qrc` uses unique name to avoid `qInitResources_*` symbol collisions with GStreamer's embedded Qt resources.

## Build

```bash
rm -fr build/ && cmake -B build -S . && cmake --build build -j
```

## Run

```bash
./build/qnujitsi
```

Enter Jitsi server, room name, and resolution in UI toolbar, then click Connect.

## Dependencies

- GStreamer 1.0+ with jitsibin plugin (from gstjitsimeet)
- Qt 6.x (static build)
- VideoToolbox (macOS hardware codec)
- libvpx, OpenSSL, libwebsockets (via jitsibin)
