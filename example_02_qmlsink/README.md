# GStreamer QML6 Sink Example with Static Libraries

This example demonstrates using the GStreamer qml6glsink plugin with statically-linked GStreamer and Qt6.

## Prerequisites

- Static GStreamer installation at `/Users/dan/code/gstreamer-build/install` (built using `build_gstreamer.sh`)
- Static Qt6 installation at `/Users/dan/qt6-static-build`
- CMake 3.16 or newer
- macOS with Xcode command line tools

## Building

```bash
cd /Users/dan/code/gstreamer-build/qmlsink

# Configure the build
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/Users/dan/qt6-static-build" \
  -DGSTREAMER_INSTALL_PREFIX="/Users/dan/code/gstreamer-build/install"

# Build the application
cmake --build build --config Release
```

The resulting executable will be at `build/qml6sink` (~62MB statically-linked).

## Running

Set the required environment variables before running:

```bash
export GST_PLUGIN_PATH="/Users/dan/code/gstreamer-build/install/lib/gstreamer-1.0"
export DYLD_LIBRARY_PATH="/Users/dan/code/gstreamer-build/install/lib:$DYLD_LIBRARY_PATH"
export QML2_IMPORT_PATH="/Users/dan/qt6-static-build/qml:$QML2_IMPORT_PATH"

./build/qml6sink
```

## Architecture

The application uses:

- **Static GStreamer** - Built with Meson, includes:
  - `libgstreamer-full-1.0.a` - Main static library
  - `libgstqml6.a` - QML6 plugin (qml6glsink)
  - All GStreamer plugins statically compiled in
  
- **Static Qt6** - Qt6 libraries linked statically with QML support

- **Static Plugin Registration** - The application calls `gst_init_static_plugins()` after `gst_init()` to register statically-linked plugins

## Key Implementation Details

### CMakeLists.txt Features

1. **Dual pkg-config paths** - Searches both main and plugin pkgconfig directories:
   ```cmake
   set(ENV{PKG_CONFIG_PATH} "${GSTREAMER_INSTALL_PREFIX}/lib/pkgconfig:${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0/pkgconfig")
   ```

2. **Explicit qml6 plugin linking** - The qml6 plugin is separate from libgstreamer-full:
   ```cmake
   ${GSTREAMER_INSTALL_PREFIX}/lib/gstreamer-1.0/libgstqml6.a
   ```

3. **Symbol aliasing for libintl** - Uses linker aliases to resolve proxy-libintl symbol naming:
   ```cmake
   -Wl,-alias,_g_libintl_gettext,_libintl_gettext
   ```

### main.cpp Modifications

- Changed from `QApplication` to `QGuiApplication` (QML apps don't need widgets)
- Added forward declaration: `extern "C" void gst_init_static_plugins(void);`
- Calls `gst_init_static_plugins()` after `gst_init(&argc, &argv)`

## Troubleshooting

### Undefined symbols for libintl_*

If you see linker errors for `_libintl_bind_textdomain_codeset`, `_libintl_bindtextdomain`, or `_libintl_gettext`:

- The GStreamer build uses proxy-libintl which prefixes symbols with `g_`
- The CMakeLists.txt includes linker aliases to resolve this (macOS only)
- Verify `libintl.a` exists at: `${GSTREAMER_INSTALL_PREFIX}/lib/libintl.a`

### QML module not found

If the application can't find QML modules:
- Ensure `QML2_IMPORT_PATH` is set correctly
- Check that Qt6 QML modules exist in the static Qt build

### Plugin registration failures

If plugins aren't available:
- Verify `gst_init_static_plugins()` is called after `gst_init()`
- Check `GST_PLUGIN_PATH` points to the plugin directory
- Use `GST_DEBUG=3 ./build/qml6sink` for detailed plugin loading information

## Build System Comparison

### Original (qmake/meson)
- Used `.pro` file with qmake
- Built against dynamic GStreamer libraries
- Simpler configuration but requires runtime GStreamer installation

### This CMakeLists.txt
- Modern CMake with `pkg-config` integration
- Static linking of all dependencies
- Self-contained ~62MB executable
- More complex but fully portable (no runtime dependencies)

## License

This example follows the GStreamer licensing (LGPL).