## qnujitsi

qnujitsi is a minimal Qt/QML application that demonstrates real-time audio/video send/receive into a Jitsi Meet conference using the custom `jitsibin` GStreamer element (from gstjitsimeet). It renders video via `qml6glsink` using Qt Quick's OpenGL backend and publishes local test AV.

## Build
rm -fr build/ && cmake -B build -S . && cmake --build build -j
