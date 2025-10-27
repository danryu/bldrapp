cmake_minimum_required(VERSION 3.14)
project(GstTestApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the PKG_CONFIG_PATH to point to your custom GStreamer installation
set(ENV{PKG_CONFIG_PATH} "/Users/dan/code/gstreamer-build/install/lib/pkgconfig:/Users/dan/code/gstreamer-build/install/lib/gstreamer-1.0/pkgconfig:$ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-full-1.0>=1.20)

add_executable(gst_test main.cpp)
target_include_directories(gst_test PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_directories(gst_test PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
    /opt/homebrew/lib
)

# Use LINK_LIBRARIES which properly handles frameworks and libraries from pkg-config
target_link_libraries(gst_test PRIVATE
    ${GSTREAMER_LINK_LIBRARIES}
    # Add missing system libraries from Homebrew
    /opt/homebrew/lib/libogg.dylib
    /opt/homebrew/lib/libX11.dylib
    /opt/homebrew/lib/libsndfile.dylib
    /opt/homebrew/lib/libaom.dylib
    /opt/homebrew/lib/libharfbuzz.dylib
    /opt/homebrew/lib/libssl.dylib
    /opt/homebrew/lib/libcrypto.dylib
    lzma  # System liblzma
    z     # System zlib
)

# Add stub implementations for missing GIO modules and QML6 plugin
# These are dynamically loaded modules that shouldn't be statically linked
target_sources(gst_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/stub_modules.c)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/stub_modules.c "
// Stub implementations for dynamically loaded modules
void g_io_openssl_load(void) {}
void g_io_environmentproxy_load(void) {}
void gst_plugin_qml6_register(void) {}
")

target_compile_options(gst_test PRIVATE ${GSTREAMER_CFLAGS_OTHER})
