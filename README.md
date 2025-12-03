# GStreamer Build with qml6glsink Plugin

This directory creates a complete STATIC GStreamer build from source with:
- Qt6 QML support, specifically including the essential **qml6glsink** plugin.
- support for RTP etc for Jitsi (jitsibin)
- support for hardware h264 and software Opus codecs

## Build Information

- **GStreamer Version**: 1.24.9
- **Qt Version**: Qt 6.5.7 (custom static build)
- **Build Date**: October 2025
- **Platform**: macOS (Apple Silicon)


## Requirements

- Static Qt build as per kormix-app/ qt-static-build github workflow
- meson
- ... ?

## pkgconfig .pc files 

These are needed to allow qt6 detection. I think.

Tracked items:

- `pkgconfig/*.pc`: pkgconfig files for Qt6Core etc

---