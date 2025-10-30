#!/bin/bash
set -e

# GStreamer Build Script for macOS with qml6glsink plugin
# Static build using STATIC Qt at /Users/dan/qt6-static-build

echo "=================================================="
echo "GStreamer Build Script with qml6glsink Support"
echo "=================================================="

# Configuration
BUILD_DIR="$(pwd)"
INSTALL_PREFIX="${BUILD_DIR}/install"

# Qt6 Configuration (STATIC QT)
QT_PATH="/Users/dan/qt6-static-build"

echo "Build directory: ${BUILD_DIR}"
echo "Install prefix: ${INSTALL_PREFIX}"
echo "Qt6 path: ${QT_PATH}"

# Step 1: (Optional) Install build dependencies via Homebrew
echo ""
echo "Step 1: Checking dependencies (Homebrew install disabled by default)..."
INSTALL_DEPS=${INSTALL_DEPS:-false}
if [ "${INSTALL_DEPS}" = true ]; then
	brew install meson ninja pkg-config python3 bison flex nasm gettext
	# brew install cairo jpeg libpng opus libvpx jack speex flac lame \
	# 	mpg123 libdv libnice json-glib libsoup openssl srtp \
	# 	libde265 aom webp libsndfile srt curl
else
	echo "Skipping Homebrew installs. Ensure meson, ninja, pkg-config, python3, bison, flex are installed."
fi

# Step 2: Clone GStreamer repositories
echo ""
echo "Step 2: Cloning GStreamer repositories..."
if [ ! -d "gstreamer" ]; then
	git clone https://gitlab.freedesktop.org/gstreamer/gstreamer.git
	cd gstreamer
	# Use a stable release
	git checkout 1.24.9 
	cd ..
else
	echo "GStreamer repository already exists, skipping clone"
fi


# Step 3b: Install Qt6 pkg-config files from repository templates
echo ""
echo "Step 3b: Installing Qt6 pkg-config files (Core/Gui/Qml/Quick)..."
QT_VER_PKG="6.5.7"
QT_PCDIR="${QT_PATH}/lib/pkgconfig"
mkdir -p "${QT_PCDIR}"
for pc in Qt6Core.pc Qt6Gui.pc Qt6Qml.pc Qt6Quick.pc; do
	if [ -f "${BUILD_DIR}/pkgconfig/${pc}" ]; then
		sed -e "s#@QT_PREFIX@#${QT_PATH}#g" -e "s#@QT_VERSION@#${QT_VER_PKG}#g" \
			"${BUILD_DIR}/pkgconfig/${pc}" > "${QT_PCDIR}/${pc}"
	else
		echo "WARNING: Missing ${BUILD_DIR}/pkgconfig/${pc}; skipping"
	fi
done
export PKG_CONFIG_PATH="${QT_PCDIR}:${PKG_CONFIG_PATH}"
echo "Installed pkg-config files to: ${QT_PCDIR}"

# Step 4: Configure Meson build with Qt6 support
echo ""
echo "Step 4: Configuring build with Meson..."
cd gstreamer

# Clean previous build if it exists
if [ -d "builddir" ]; then
	echo "Removing previous build directory..."
	rm -rf builddir
fi


# Set up environment for Qt6 (STATIC)
export PKG_CONFIG_PATH="${QT_PATH}/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="${QT_PATH}/lib/pkgconfig"
export PATH="${QT_PATH}/bin:${QT_PATH}/libexec:${PATH}"
export CMAKE_PREFIX_PATH="${QT_PATH}"

# Ensure our local wrap is installed for proxy-libintl fallback
if [ -f "${BUILD_DIR}/wraps/proxy-libintl.wrap" ]; then
	mkdir -p "subprojects"
	cp -f "${BUILD_DIR}/wraps/proxy-libintl.wrap" "subprojects/proxy-libintl.wrap"
fi

# Download proxy-libintl
meson subprojects download proxy-libintl || true

# Ensure proxy-libintl overrides dependency('intl') even if upstream changes
PROXY_INTL_DIR="subprojects/proxy-libintl"
if [ -d "${PROXY_INTL_DIR}" ]; then
	if ! grep -q "override_dependency('intl'" "${PROXY_INTL_DIR}/meson.build" 2>/dev/null; then
		if [ -f "${BUILD_DIR}/patches/proxy-libintl/meson.build.append" ]; then
			printf "\n%s\n" "$(cat "${BUILD_DIR}/patches/proxy-libintl/meson.build.append")" >> "${PROXY_INTL_DIR}/meson.build"
		fi
	fi
fi

# Configure with Meson (STATIC build)
# Enable gst-full (monolithic static library)
# Use wildcard for plugins to include all enabled plugins

meson setup builddir \
	--prefix="${INSTALL_PREFIX}" \
	--buildtype=release \
	-Dcmake_prefix_path="${QT_PATH}" \
	--default-library=static \
	--force-fallback-for=gstreamer-1.0,glib,libffi,pcre2,proxy-libintl \
	-Dauto_features=disabled \
	-Dglib:tests=false \
	-Djson-glib:tests=false \
	-Dpcre2:test=false \
	-Dgstreamer-1.0:libav=disabled \
	-Dgstreamer-1.0:ugly=disabled \
	-Dgstreamer-1.0:ges=disabled \
	-Dgstreamer-1.0:devtools=disabled \
	-Dglib:nls=disabled \
	-Dgstreamer-1.0:default_library=static \
	-Dgstreamer-1.0:rtsp_server=disabled \
	-Dgst-full=enabled \
	-Dgst-full-target-type=static_library \
	-Dgst-full-libraries=gstreamer-video-1.0,gstreamer-audio-1.0,gstreamer-app-1.0,gstreamer-gl-1.0,gstreamer-base-1.0,gstreamer-tag-1.0,gstreamer-pbutils-1.0,gstreamer-rtp-1.0 \
	-Dgstreamer-1.0:tools=disabled \
	-Dgst-plugins-base:gl=enabled \
	-Dgst-plugins-base:playback=enabled \
	-Dgst-plugins-base:app=enabled \
	-Dgst-plugins-base:videoconvertscale=enabled \
	-Dgst-plugins-base:videotestsrc=enabled \
	-Dgst-plugins-base:audioresample=enabled \
	-Dgst-plugins-base:audioconvert=enabled \
	-Dgst-plugins-base:audiotestsrc=enabled \
	-Dgst-plugins-base:typefind=enabled \
	-Dgst-plugins-base:rawparse=enabled \
	-Dgst-plugins-good:rtp=enabled \
	-Dgst-plugins-good:rtpmanager=enabled \
	-Dgst-plugins-good:rtsp=enabled \
	-Dgst-plugins-good:udp=enabled \
	-Dgst-plugins-good:videofilter=enabled \
	-Dgst-plugins-good:videomixer=enabled \
	-Dgst-plugins-good:audioparsers=enabled \
	-Dgst-plugins-good:autodetect=enabled \
	-Dgst-plugins-bad:dtls=enabled \
	-Dgst-plugins-bad:srtp=enabled \
	-Dgst-plugins-bad:videoparsers=enabled \
	-Dgst-plugins-bad:aom=enabled \
	-Dlibnice=enabled \
	-Dlibnice:crypto-library=openssl \
	-Dlibnice:gstreamer=enabled \
	-Dlibnice:tests=disabled \
	-Dlibnice:examples=disabled \
	-Dlibnice:introspection=disabled \
	-Dgst-plugins-base:opus=enabled \
	-Dqt6=enabled \
	-Dgst-plugins-good:qt6=enabled


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

# Step 7: Verify artifacts (qml6 plugin and libgstreamer-full static lib)
echo ""
echo "Step 7: Verifying qml6 plugin/artifacts..."
export PATH="${INSTALL_PREFIX}/bin:$PATH"
export GST_PLUGIN_PATH="${INSTALL_PREFIX}/lib/gstreamer-1.0"

echo "Looking for qml6 plugin artifacts under ${INSTALL_PREFIX}..."
QML6_ANY=$(find "${INSTALL_PREFIX}" -name 'libgstqml6*.*' -print 2>/dev/null || true)
if [ -n "${QML6_ANY}" ]; then
	echo "Found qml6 plugin artifacts:"
	echo "${QML6_ANY}"
else
	echo "ERROR: qml6 plugin not found in install prefix."
	(ls -la "${INSTALL_PREFIX}/lib/gstreamer-1.0" || true)
	exit 1
fi

echo "Looking for libgstreamer-full-1.0.a under ${INSTALL_PREFIX}/lib..."
GST_FULL_A="${INSTALL_PREFIX}/lib/libgstreamer-full-1.0.a"
if [ -f "${GST_FULL_A}" ]; then
	echo "Found static lib: ${GST_FULL_A}"
	echo "Size: $(ls -lh "${GST_FULL_A}" | awk '{print $5}')"
else
	echo "ERROR: libgstreamer-full-1.0.a not found."
	echo "Available libraries:"
	(ls -lh "${INSTALL_PREFIX}/lib"/*.a 2>/dev/null || true)
	exit 1
fi

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
echo "  ls -la ${INSTALL_PREFIX}/lib/libgstreamer-full-1.0.a"
echo "  find ${INSTALL_PREFIX} -name 'libgstqml6*.*'"
echo ""
echo "Key components installed:"
echo "  - GStreamer core libraries (static libgstreamer-full-1.0.a)"
echo "  - Qt6 QML plugin (qml6glsink)"
echo "  - Tools (gst-inspect, gst-launch, etc.)"
echo ""