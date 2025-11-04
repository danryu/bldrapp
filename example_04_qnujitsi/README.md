## qnujitsi

qnujitsi is a minimal Qt/QML application that demonstrates real-time audio/video send/receive into a Jitsi Meet conference using the custom `jitsibin` GStreamer element (from gstjitsimeet). It renders video via `qml6glsink` using Qt Quick’s OpenGL backend and publishes local test AV.

### High-level overview
- **UI**: Qt Quick window with a QML item used by `qml6glsink` to display GL textures.
- **Network/Signaling**: `jitsibin` handles XMPP signaling, ICE, DTLS-SRTP, RTP/RTCP, and payloaders/depayloaders.
- **Receive path**: `jitsibin → queue (leaky) → decodebin → queue (leaky) → videoconvert → glupload → qml6glsink`.
- **Send path**: `videotestsrc → videoconvert → av1enc → av1parse → jitsibin:video_sink` and `audiotestsrc → opusenc → jitsibin:audio_sink`.

### Why a leaky queue before and after decode?
- The queues decouple the live network from the (potentially) slow/blocked decoder and rendering path.
- Both queues are configured as **downstream-leaky** to drop late frames rather than stall the pipeline.

### Key resilience features
To handle corrupt frames and keep the pipeline responsive:
- **Leaky pre-decode queue**: Between `jitsibin` and `decodebin` to absorb jitter and prevent upstream back-pressure when decode stalls.
- **Leaky post-decode queue**: Between `decodebin` and `videoconvert` to drop late frames and prevent render blocking.
- **Bus draining**: A Qt `QTimer` periodically drains the `GstBus` to avoid queue overflow/back-pressure from unhandled messages.
- **Keyframe watchdog**: If no decoded video buffers are observed for ~700 ms, send an upstream `GstForceKeyUnit` event (PLI/FIR) to request an IDR/intra frame.
- **Event throttling**: Keyframe requests are throttled (≤2/sec) to avoid RTCP spam.
- **Moderate bus polling**: Bus polling runs ~60 Hz and caps per-tick message handling to keep CPU usage reasonable.

### Pipeline construction details
1) Create core elements: `pipeline`, `jitsibin`, `videoconvert`, `glupload`, `qml6glsink`.
2) Create and link the static tail of the receive chain: `videoconvert → glupload → qml6glsink`.
3) Connect to `jitsibin`’s `pad-added` signal:
   - When a new RTP stream arrives, create a leaky `queue` and `decodebin`.
   - Link: `jitsibin:src_pad → queue:sink → decodebin:sink`.
   - Hook `decodebin`’s `pad-added` signal:
     - For video pads, create a post-decode leaky `queue`.
     - Link: `decodebin:src_pad → queue:sink → videoconvert`.
4) Set `qml6glsink`’s `widget` property to the QML item.
5) Start the pipeline from the Qt render thread to ensure GL context correctness.
6) Add timers:
   - **Bus drain timer** (≈16 ms): pop and process bus messages up to a cap each tick.
   - **Watchdog timer** (200 ms): if no decoded buffers for ~700 ms, send upstream force-key-unit.

### Resilience parameters (tune as needed)
- Pre-decode queue: `leaky=downstream`, `max-size-buffers=50`.
- Post-decode queue: `leaky=downstream`, `max-size-buffers=5`.
- Watchdog gap: 700 ms without decoded buffers triggers keyframe request.
- Keyframe request throttle: ≥500 ms between requests.
- Bus drain: ~60 Hz, up to 200 messages per tick.

### CPU considerations
- Avoid a busy bus-drain loop; use moderate timer cadence and per-tick cap.
- AV1 encoding is expensive; for receive-only tests, disable the send path or use a cheaper encoder.

### Debug tips
- Environment: `GST_DEBUG=3` (and add `videodecoder:4,rtpsession:4,rtpbin:5` for deeper insight).
- Look for: “jitsibin → queue link OK”, “decodebin → queue link OK”, and watchdog “Requested keyframe … OK”.

### Known limitations
- Recovery depends on the remote endpoint honoring PLI/FIR or sending periodic keyframes.
- Extremely long gaps or persistent corruption may require additional application policy (e.g., re-negotiate).

### Build prerequisites
- Built against a custom GStreamer with `qml6glsink` and Qt 6.x from Homebrew; see `CMakeLists.txt` for pkg-config paths and Qt policies.
