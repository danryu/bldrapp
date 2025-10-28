# GStreamer Build with qml6glsink Plugin

This directory contains a complete GStreamer build from source with Qt6 QML support, specifically including the essential **qml6glsink** plugin.

## Build Information

- **GStreamer Version**: 1.24.9
- **Qt Version**: Qt 6.9.3 (Homebrew)
- **Build Date**: October 2025
- **Platform**: macOS (Apple Silicon + Intel)

## Installation Paths

```
gstreamer-build/
├── install/                          # Installation directory
│   ├── bin/                          # GStreamer binaries and tools
│   │   ├── gst-inspect-1.0          # Plugin inspection tool
│   │   ├── gst-launch-1.0           # Pipeline launcher
│   │   ├── gst-play-1.0             # Simple media player
│   │   └── ...                       # Other tools
│   ├── lib/
│   │   ├── gstreamer-1.0/           # GStreamer plugins
│   │   │   ├── libgstqml6.dylib    # Qt6 QML plugin (includes qml6glsink)
│   │   │   └── ...                  # Other plugins
│   │   ├── libgstreamer-1.0.dylib   # Core library
│   │   ├── libgstbase-1.0.dylib     # Base library
│   │   └── pkgconfig/               # pkg-config files
│   ├── include/gstreamer-1.0/       # Development headers
│   └── share/                        # Data files
├── gstreamer/                        # Source code
│   ├── builddir/                     # Build artifacts
│   └── subprojects/                  # Dependencies
├── build_gstreamer.sh                # Complete build script
├── meson-native.ini                  # Meson configuration for Qt private headers
├── build.log                         # Build output log
└── README.md                         # This file
```

## proxy-libintl fallback for GLib (CI reproducibility)

GLib requires gettext (`libintl`). To avoid requiring a system gettext in local and CI builds, this repo vendors a Meson wrap and a tiny override snippet so Meson resolves `dependency('intl')` to the bundled `proxy-libintl` subproject.

Tracked items:

- `wraps/proxy-libintl.wrap`: Local wrap that declares it provides `intl`.
- `patches/proxy-libintl/meson.build.append`: Appended to the subproject `meson.build` to call `meson.override_dependency('intl', intl_dep)`.
- `build_gstreamer.sh`: Copies the wrap into `gstreamer/subprojects/`, pre-downloads the subproject, and appends the override if missing.

This makes builds self-contained and prevents Meson from failing with “Dependency "intl" not found”.

## Using the Built GStreamer

### Environment Setup

To use this GStreamer build in your shell:

```bash
export PATH="/Users/dan/code/kormix-app/gstreamer-build/install/bin:$PATH"
export GST_PLUGIN_PATH="/Users/dan/code/kormix-app/gstreamer-build/install/lib/gstreamer-1.0"
export PKG_CONFIG_PATH="/Users/dan/code/kormix-app/gstreamer-build/install/lib/pkgconfig:$PKG_CONFIG_PATH"
export DYLD_LIBRARY_PATH="/Users/dan/code/kormix-app/gstreamer-build/install/lib:$DYLD_LIBRARY_PATH"
```

### Verification

Verify the qml6glsink plugin is available:

```bash
gst-inspect-1.0 qml6glsink
```

Expected output should show:
```
Factory Details:
  Rank                     none (0)
  Long-name                Qt6 Video Sink
  Klass                    Sink/Video
  Description              A video sink that renders to a QQuickItem for Qt6
  ...
```

### Using qml6glsink in Pipelines

Example pipeline using qml6glsink:

```bash
gst-launch-1.0 videotestsrc ! qml6glsink
```

## CMake Integration

To use this GStreamer build in your CMake project:

```cmake
# Add to your CMakeLists.txt
set(ENV{PKG_CONFIG_PATH} "/Users/dan/code/kormix-app/gstreamer-build/install/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

target_include_directories(your_target PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(your_target PRIVATE ${GSTREAMER_LIBRARIES})
```

## Rebuilding from Scratch

To rebuild GStreamer:

```bash
cd /Users/dan/code/kormix-app/gstreamer-build
./build_gstreamer.sh
```

The script will:
1. Install all required dependencies via Homebrew
2. Clone GStreamer source code (if not already present)
3. Create Meson native file for Qt6 private headers
4. Configure the build with Qt6 support enabled
5. Build all GStreamer components (3178 targets)
6. Install to `./install` directory
7. Verify qml6glsink plugin is working

## Key Features

### Included Plugin Sets
- **gst-plugins-base**: Essential plugins (audio, video, networking)
- **gst-plugins-good**: Well-maintained, high-quality plugins
- **gst-plugins-bad**: Experimental or lesser-known plugins
- **gst-plugins-ugly**: Plugins with licensing concerns
- **gst-libav**: FFmpeg-based plugins
- **gst-plugins-qt**: Qt6 integration plugins (qml6glsink)

### Notable Plugins
- `qml6glsink` - Qt6 QML video sink (primary reason for this build)
- `osxaudio` - macOS CoreAudio support
- `osxvideo` - macOS video output
- `applemedia` - Apple hardware codecs
- OpenGL/Metal rendering support
- WebRTC support
- RTSP streaming

### Disabled Plugins
The following plugins were disabled due to API incompatibilities:
- `svtav1` - Incompatible with Homebrew svt-av1 3.1.2
- `x265` - API incompatibility with x265 4.1

## Dependencies

All dependencies are managed via Homebrew:

```bash
# Build tools
meson ninja pkg-config python3 bison flex nasm gettext

# Libraries
qt@6 cairo jpeg libpng opus libvpx x264 jack speex flac lame dv
mpg123 libdv libnice json-glib libsoup openssl libsrtp
libde265 openh264 aom webp libsndfile srt curl
```

## Qt6 Private Headers

The build requires Qt6 private headers for the qml6glsink plugin. These are configured via `meson-native.ini`:

```ini
[built-in options]
cpp_args = [
    '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtGui.framework/Versions/A/Headers/6.9.3/QtGui',
    '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtCore.framework/Versions/A/Headers/6.9.3/QtCore',
    '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtQml.framework/Versions/A/Headers/6.9.3/QtQml',
    '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtQuick.framework/Versions/A/Headers/6.9.3/QtQuick'
]
```

## Troubleshooting

### Plugin Not Found

If `gst-inspect-1.0 qml6glsink` fails:
1. Check environment variables are set correctly
2. Verify plugin file exists: `ls -la install/lib/gstreamer-1.0/libgstqml6.dylib`
3. Check plugin path: `echo $GST_PLUGIN_PATH`

### Library Loading Issues

If you see dylib loading errors:
```bash
export DYLD_LIBRARY_PATH="/Users/dan/code/kormix-app/gstreamer-build/install/lib:$DYLD_LIBRARY_PATH"
```

### Qt Version Changes

If Homebrew updates Qt, you may need to update the paths in `meson-native.ini` and rebuild:
```bash
# Find new Qt version
brew info qt@6

# Update paths in meson-native.ini
# Then rebuild
cd gstreamer && rm -rf builddir && cd ..
./build_gstreamer.sh
```

## Development

### Inspecting Plugins

```bash
# List all available plugins
gst-inspect-1.0

# Inspect specific plugin
gst-inspect-1.0 qml6glsink

# Test video pipeline
gst-launch-1.0 videotestsrc ! videoconvert ! qml6glsink
```

### Debugging

Enable GStreamer debug logging:
```bash
export GST_DEBUG=3  # 0=none, 5=max
export GST_DEBUG_FILE=/tmp/gst-debug.log
```

## Additional Resources

- [GStreamer Documentation](https://gstreamer.freedesktop.org/documentation/)
- [qml6glsink Plugin Documentation](https://gstreamer.freedesktop.org/documentation/qml6/qml6glsink.html)
- [Qt6 QML Documentation](https://doc.qt.io/qt-6/qtqml-index.html)

## Notes

- Build time: Approximately 15-30 minutes on Apple Silicon
- Disk space required: ~2GB for source + build + install
- The build uses Homebrew Qt rather than the static Qt build due to easier private header access
- This is a complete GStreamer build suitable for development and production use