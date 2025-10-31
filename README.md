# GStreamer Build with qml6glsink Plugin

This directory creates a complete STATIC GStreamer build from source with:
- Qt6 QML support, specifically including the essential **qml6glsink** plugin.
- support for RTP etc for Jitsi (jitsibin)
- support for AV1, Opus codecs

## Build Information

- **GStreamer Version**: 1.24.9
- **Qt Version**: Qt 6.5.7 (custom static build)
- **Build Date**: October 2025
- **Platform**: macOS (Apple Silicon)


## Requirements

- Static Qt build as per kormix-app/ qt-static-build github workflow
- meson
- ... ?

## proxy-libintl fallback for GLib (CI reproducibility)

GLib requires gettext (`libintl`). To avoid requiring a system gettext in local and CI builds, this repo vendors a Meson wrap and a tiny override snippet so Meson resolves `dependency('intl')` to the bundled `proxy-libintl` subproject.

Tracked items:

- `wraps/proxy-libintl.wrap`: Local wrap that declares it provides `intl`.
- `patches/proxy-libintl/meson.build.append`: Appended to the subproject `meson.build` to call `meson.override_dependency('intl', intl_dep)`.
- `build_gstreamer.sh`: Copies the wrap into `gstreamer/subprojects/`, pre-downloads the subproject, and appends the override if missing.

This makes builds self-contained and prevents Meson from failing with “Dependency "intl" not found”.


## pkgconfig .pc files 

These are needed to allow qt6 detection. I think.

Tracked items:

- `pkgconfig/*.pc`: pkgconfig files for Qt6Core etc

---