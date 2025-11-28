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
	git checkout 1.26.7
	cd ..
else
	echo "GStreamer repository already exists, skipping clone"
fi


# Step 3b: Install Qt6 pkg-config files from repository templates
# Seems to be necessary for qt6 detection in Gstreamer meson build.
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
export MACOSX_DEPLOYMENT_TARGET="13.3"
# # Ensure Rust and C/C++ agree on the deployment target and link system zlib (no Homebrew)
# export RUSTFLAGS="-C link-arg=-mmacosx-version-min=13.3 -C link-arg=-lz ${RUSTFLAGS}"
export CFLAGS="-mmacosx-version-min=13.3 ${CFLAGS}"
export CXXFLAGS="-mmacosx-version-min=13.3 ${CXXFLAGS}"

##############################################################################################################################
# Build OpenSSL separately with CMake before Meson configure
# This provides static SSL libraries for libwebsockets and libnice
# Configure OpenSSL to use system default CA certificate locations
##############################################################################################################################
OPENSSL_INSTALL="${BUILD_DIR}/openssl-install"
echo ""
echo "Step 3a: Building OpenSSL from source..."
if [ ! -d "${OPENSSL_INSTALL}" ]; then
	cd "${BUILD_DIR}"
	if [ ! -d "openssl-src" ]; then
		echo "Cloning OpenSSL 3.0.15..."
		git clone --depth 1 --branch openssl-3.0.15 https://github.com/openssl/openssl.git openssl-src
	fi
	cd openssl-src
	
	echo "Configuring OpenSSL to use system CA certificates..."
	# Set openssldir to system default locations where CA certs are found
	# On macOS: /etc/ssl (where cert.pem is located)
	# On Windows: OpenSSL will use Windows certificate store via CAPI engine
	# On Linux: typically /etc/ssl/certs
	./Configure darwin64-arm64-cc \
		--prefix="${OPENSSL_INSTALL}" \
		--openssldir=/etc/ssl \
		no-shared \
		no-tests \
		-mmacosx-version-min="${MACOSX_DEPLOYMENT_TARGET:-13.3}"
	
	echo "Building OpenSSL..."
	make -j$(sysctl -n hw.ncpu)
	
	echo "Installing OpenSSL to ${OPENSSL_INSTALL}..."
	make install_sw
	
	cd "${BUILD_DIR}/gstreamer"
	echo "OpenSSL build complete!"
else
	echo "OpenSSL already built at ${OPENSSL_INSTALL}"
fi

# Expose OpenSSL via PKG_CONFIG_PATH
export PKG_CONFIG_PATH="${OPENSSL_INSTALL}/lib/pkgconfig:${PKG_CONFIG_PATH}"
echo "Added OpenSSL to PKG_CONFIG_PATH: ${OPENSSL_INSTALL}/lib/pkgconfig"
##############################################################################################################################

##############################################################################################################################
# Build libvpx separately for VP8/VP9 support
##############################################################################################################################
VPX_INSTALL="${BUILD_DIR}/vpx-install"
echo ""
echo "Step 3c: Building libvpx from source..."
if [ ! -d "${VPX_INSTALL}" ]; then
	cd "${BUILD_DIR}"
	if [ ! -d "libvpx-src" ]; then
		echo "Cloning libvpx v1.14.1..."
		git clone --depth 1 --branch v1.14.1 https://chromium.googlesource.com/webm/libvpx libvpx-src
	fi
	cd libvpx-src
	
	echo "Configuring libvpx..."
	./configure \
		--prefix="${VPX_INSTALL}" \
		--disable-shared \
		--enable-static \
		--disable-examples \
		--disable-tools \
		--disable-docs \
		--disable-unit-tests \
		--enable-vp8 \
		--enable-vp9 \
		--enable-vp9-highbitdepth \
		--as=yasm
	
	echo "Building libvpx..."
	make -j$(sysctl -n hw.ncpu)
	
	echo "Installing libvpx to ${VPX_INSTALL}..."
	make install
	
	cd "${BUILD_DIR}/gstreamer"
	echo "libvpx build complete!"
else
	echo "libvpx already built at ${VPX_INSTALL}"
fi

# Expose libvpx via PKG_CONFIG_PATH
export PKG_CONFIG_PATH="${VPX_INSTALL}/lib/pkgconfig:${PKG_CONFIG_PATH}"
echo "Added libvpx to PKG_CONFIG_PATH: ${VPX_INSTALL}/lib/pkgconfig"
##############################################################################################################################

##############################################################################################################################
# Build libwebsockets separately with CMake before Meson configure
# This avoids Meson/CMake integration issues and ensures a clean static build
##############################################################################################################################
LWS_INSTALL="${BUILD_DIR}/libwebsockets-install"
echo ""
echo "Step 3e: Building libwebsockets from source..."
if [ ! -d "${LWS_INSTALL}" ]; then
	cd "${BUILD_DIR}"
	if [ ! -d "libwebsockets-src" ]; then
		echo "Cloning libwebsockets v4.3.6..."
		git clone --depth 1 --branch v4.3.6 https://github.com/warmcat/libwebsockets.git libwebsockets-src
	fi
	cd libwebsockets-src
	
	echo "Configuring libwebsockets with CMake..."
	cmake -B build \
	  	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="${LWS_INSTALL}" \
		-DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.3}" \
		-DOPENSSL_ROOT_DIR="${OPENSSL_INSTALL}" \
		-DOPENSSL_INCLUDE_DIR="${OPENSSL_INSTALL}/include" \
		-DOPENSSL_CRYPTO_LIBRARY="${OPENSSL_INSTALL}/lib/libcrypto.a" \
		-DOPENSSL_SSL_LIBRARY="${OPENSSL_INSTALL}/lib/libssl.a" \
		-DLWS_WITH_SHARED=OFF \
		-DLWS_WITH_STATIC=ON \
		-DLWS_WITHOUT_TESTAPPS=ON \
		-DLWS_WITHOUT_TEST_SERVER=ON \
		-DLWS_WITHOUT_TEST_PING=ON \
		-DLWS_WITHOUT_TEST_CLIENT=ON \
		-DLWS_WITH_SSL=ON \
		-DLWS_OPENSSL_SUPPORT=ON \
		-DLWS_WITH_ZLIB=OFF \
		-DLWS_WITHOUT_EXTENSIONS=ON \
		-DLWS_IPV6=ON \
		-DLWS_UNIX_SOCK=ON \
		-DLWS_WITH_LIBUV=OFF \
		-DLWS_WITH_LIBEVENT=OFF \
		-DLWS_WITH_GLIB=OFF \
		-DLWS_WITH_HTTP2=OFF \
		-DLWS_WITH_MINIMAL_EXAMPLES=OFF \
		-DLWS_LINK_TESTAPPS_DYNAMIC=OFF
	
	echo "Building libwebsockets..."
	cmake --build build --config Release -j$(sysctl -n hw.ncpu)
	
	echo "Installing libwebsockets to ${LWS_INSTALL}..."
	cmake --install build
	
	cd "${BUILD_DIR}/gstreamer"
	echo "libwebsockets build complete!"
else
	echo "libwebsockets already built at ${LWS_INSTALL}"
fi

# Expose libwebsockets via PKG_CONFIG_PATH
export PKG_CONFIG_PATH="${LWS_INSTALL}/lib/pkgconfig:${PKG_CONFIG_PATH}"
echo "Added libwebsockets to PKG_CONFIG_PATH: ${LWS_INSTALL}/lib/pkgconfig"
##############################################################################################################################


##############################################################################################################################

# Ensure our local wraps are installed for fallbacks/custom subprojects
if [ -f "${BUILD_DIR}/wraps/proxy-libintl.wrap" ]; then
	mkdir -p "subprojects"
	cp -f "${BUILD_DIR}/wraps/proxy-libintl.wrap" "subprojects/proxy-libintl.wrap"
fi
if [ -f "${BUILD_DIR}/wraps/gstjitsimeet.wrap" ]; then
	mkdir -p "subprojects"
	cp -f "${BUILD_DIR}/wraps/gstjitsimeet.wrap" "subprojects/gstjitsimeet.wrap"
fi


# Link local gstjitsimeet directory into subprojects to avoid any VCS/network usage
GSTJ_LOCAL="/Users/dan/code/gstjitsimeet"
if [ -d "${GSTJ_LOCAL}" ] && [ ! -d "subprojects/gstjitsimeet" ]; then
	ln -s "${GSTJ_LOCAL}" "subprojects/gstjitsimeet"
fi

# If gstjitsimeet's local coop install exists, expose its pkg-config path
GSTJ_COOP_PC="$(pwd)/subprojects/gstjitsimeet/deps/coop-install/lib/pkgconfig"
if [ -d "${GSTJ_COOP_PC}" ]; then
	export PKG_CONFIG_PATH="${GSTJ_COOP_PC}:${PKG_CONFIG_PATH}"
fi
##############################################################################################################################

##############################################################################################################################
# Download proxy-libintl; gstjitsimeet is provided locally via symlink, libwebsockets pre-built
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
##############################################################################################################################

# Configure with Meson (STATIC build)
# Enable gst-full (monolithic static library)
# Use wildcard for plugins to include all enabled plugins

meson setup builddir \
	--prefix="${INSTALL_PREFIX}" \
	--buildtype=release \
	-Dcmake_prefix_path="${QT_PATH}" \
	--default-library=static \
	-Dcustom_subprojects=gstjitsimeet \
	--force-fallback-for=gstreamer-1.0,glib,libffi,pcre2,proxy-libintl \
	-Dauto_features=disabled \
	-Dglib:tests=false \
	-Djson-glib:tests=false \
	-Dpcre2:test=false \
	-Dgstreamer-1.0:ugly=disabled \
	-Dgstreamer-1.0:ges=disabled \
	-Dgstreamer-1.0:devtools=disabled \
	-Dglib:nls=disabled \
	-Dgstreamer-1.0:default_library=static \
	-Dgstreamer-1.0:rtsp_server=disabled \
	-Dgst-full=enabled \
	-Dgst-full-target-type=static_library \
	-Dgst-full-libraries=gstreamer-video-1.0,gstreamer-audio-1.0,gstreamer-app-1.0,gstreamer-gl-1.0,gstreamer-base-1.0,gstreamer-tag-1.0,gstreamer-pbutils-1.0,gstreamer-rtp-1.0,gstreamer-codecparsers-1.0 \
	-Dgstreamer-1.0:tools=disabled \
	-Dgst-plugins-base:gl=enabled \
	-Dgst-plugins-base:playback=enabled \
	-Dgst-plugins-base:app=enabled \
	-Dgst-plugins-base:videorate=enabled \
	-Dgst-plugins-base:videoconvertscale=enabled \
	-Dgst-plugins-base:videotestsrc=enabled \
	-Dgst-plugins-base:audioresample=enabled \
	-Dgst-plugins-base:audioconvert=enabled \
	-Dgst-plugins-base:audiotestsrc=enabled \
	-Dgst-plugins-base:volume=enabled \
	-Dgst-plugins-base:typefind=enabled \
	-Dgst-plugins-base:rawparse=enabled \
	-Dgst-plugins-base:opus=enabled \
	-Dgst-plugins-good:rtp=enabled \
	-Dgst-plugins-good:rtpmanager=enabled \
	-Dgst-plugins-good:rtsp=enabled \
	-Dgst-plugins-good:udp=enabled \
	-Dgst-plugins-good:videofilter=enabled \
	-Dgst-plugins-good:videomixer=enabled \
	-Dgst-plugins-good:audioparsers=enabled \
	-Dgst-plugins-good:autodetect=enabled \
	-Dgst-plugins-good:vpx=enabled \
	-Dgst-plugins-good:osxvideo=enabled \
	-Dgst-plugins-good:osxaudio=enabled \
	-Dgst-plugins-good:qt6=enabled \
	-Dgst-plugins-bad:dtls=enabled \
	-Dgst-plugins-bad:srtp=enabled \
	-Dgst-plugins-bad:videoparsers=enabled \
	-Dgst-plugins-bad:applemedia=enabled \
	-Dlibnice=enabled \
	-Dlibnice:crypto-library=openssl \
	-Dlibnice:gstreamer=enabled \
	-Dlibnice:tests=disabled \
	-Dlibnice:examples=disabled \
	-Dlibnice:introspection=disabled \
	-Dqt6=enabled

# REMOVED !!!!
	# -Drs=enabled \
	# -Dgst-plugins-rs:rtp=enabled \
	# -Dgst-plugins-rs:dav1d=enabled \
	# -Dgst-plugins-rs:rav1e=enabled \
	# -Dgst-plugins-bad:aom=enabled \
	# -Dgst-plugins-bad:svtav1=enabled \
	# -Dgst-plugins-bad:openh264=enabled \
	# -Dgstreamer-1.0:libav=enabled \


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
echo "To use this GStreamer build, follow example in qmlsink/"
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