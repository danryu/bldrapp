#!/bin/bash
set -e

# GStreamer Build Script for macOS with qml6glsink plugin
# This script builds GStreamer from source with Qt6 QML support

echo "=================================================="
echo "GStreamer Build Script with qml6glsink Support"
echo "=================================================="

# Configuration
BUILD_DIR="$(pwd)"
INSTALL_PREFIX="${BUILD_DIR}/install"

# Qt6 Configuration
# IMPORTANT: Must match the Qt version used by qjitsi
# qjitsi uses Qt 6.9.2 from /Users/dan/Qt/6.9.2/macos
QT_PATH="/Users/dan/Qt/6.9.2/macos"

# Option 2: Use Homebrew Qt (only if qjitsi also uses Homebrew Qt)
# QT_PATH="$(brew --prefix qt@6)"

echo "Build directory: ${BUILD_DIR}"
echo "Install prefix: ${INSTALL_PREFIX}"
echo "Qt6 path: ${QT_PATH}"

# Step 1: Install build dependencies via Homebrew
echo ""
echo "Step 1: Installing build dependencies..."
brew install meson ninja pkg-config python3 bison flex nasm gettext

# Install Qt6 dependencies for qml6glsink
echo "Installing Qt6 and additional dependencies..."
brew install qt@6 cairo jpeg libpng opus libvpx x264 jack speex flac lame dv \
    mpg123 libdv libnice json-glib libsoup openssl libsrtp \
    libde265 openh264 aom webp libsndfile srt curl

# Step 2: Clone GStreamer repositories
echo ""
echo "Step 2: Cloning GStreamer repositories..."
if [ ! -d "gstreamer" ]; then
    git clone https://gitlab.freedesktop.org/gstreamer/gstreamer.git
    cd gstreamer
    # Use the latest stable release
    git checkout 1.24.9 || git checkout main
    cd ..
else
    echo "GStreamer repository already exists, skipping clone"
fi

# Step 3: Create Meson native file for Qt private headers
echo ""
echo "Step 3: Creating Meson native file for Qt6 private headers..."
cat > meson-native.ini << 'EOF'
[built-in options]
cpp_args = ['-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtGui.framework/Versions/A/Headers/6.9.3/QtGui', '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtCore.framework/Versions/A/Headers/6.9.3/QtCore', '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtQml.framework/Versions/A/Headers/6.9.3/QtQml', '-I/opt/homebrew/Cellar/qt/6.9.3/lib/QtQuick.framework/Versions/A/Headers/6.9.3/QtQuick']
EOF

echo "Meson native file created: meson-native.ini"

# Step 4: Configure Meson build with Qt6 support
echo ""
echo "Step 4: Configuring build with Meson..."
cd gstreamer

# Clean previous build if it exists
if [ -d "builddir" ]; then
    echo "Removing previous build directory..."
    rm -rf builddir
fi

# Set up environment for Qt6
export PKG_CONFIG_PATH="${QT_PATH}/lib/pkgconfig:${PKG_CONFIG_PATH}"
export PATH="${QT_PATH}/bin:/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/flex/bin:${PATH}"
export CMAKE_PREFIX_PATH="${QT_PATH}"

# Configure with Meson
# Enable Qt6 support and qml6glsink plugin explicitly
# Disable incompatible plugins: svtav1 and x265
meson setup builddir \
    --native-file ../meson-native.ini \
    --prefix="${INSTALL_PREFIX}" \
    --buildtype=release \
    -Dgpl=enabled \
    -Dqt5=disabled \
    -Dqt6=enabled \
    -Dgst-plugins-good:qt6=enabled \
    -Dgst-plugins-bad:svtav1=disabled \
    -Dgst-plugins-bad:x265=disabled \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Ddoc=disabled \
    -Dintrospection=disabled

echo ""
echo "Meson configuration complete"

# Step 5: Build GStreamer
echo ""
echo "Step 5: Building GStreamer (this will take a while)..."
ninja -C builddir 2>&1 | tee ../build.log

echo ""
echo "Build completed successfully!"

# Step 6: Install GStreamer
echo ""
echo "Step 6: Installing GStreamer..."
ninja -C builddir install

echo ""
echo "Installation complete!"

# Step 7: Verify qml6glsink plugin
echo ""
echo "Step 7: Verifying qml6glsink plugin..."
export PATH="${INSTALL_PREFIX}/bin:$PATH"
export GST_PLUGIN_PATH="${INSTALL_PREFIX}/lib/gstreamer-1.0"

echo ""
echo "Testing qml6glsink plugin:"
gst-inspect-1.0 qml6glsink | head -20

echo ""
echo "=================================================="
echo "BUILD SUCCESSFUL!"
echo "=================================================="
echo ""
echo "GStreamer has been built and installed with qml6glsink plugin support."
echo ""
echo "Installation directory: ${INSTALL_PREFIX}"
echo ""
echo "To use this GStreamer build, add these to your environment:"
echo "  export PATH=\"${INSTALL_PREFIX}/bin:\$PATH\""
echo "  export GST_PLUGIN_PATH=\"${INSTALL_PREFIX}/lib/gstreamer-1.0\""
echo "  export PKG_CONFIG_PATH=\"${INSTALL_PREFIX}/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo "  export DYLD_LIBRARY_PATH=\"${INSTALL_PREFIX}/lib:\$DYLD_LIBRARY_PATH\""
echo ""
echo "Verify the installation:"
echo "  gst-inspect-1.0 qml6glsink"
echo ""
echo "Key components installed:"
echo "  - GStreamer core libraries"
echo "  - All plugin sets (base, good, bad, ugly, libav)"
echo "  - Qt6 QML plugin (qml6glsink)"
echo "  - Development tools (gst-inspect, gst-launch, etc.)"
echo ""