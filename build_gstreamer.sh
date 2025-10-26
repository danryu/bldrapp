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
	brew install cairo jpeg libpng opus libvpx x264 jack speex flac lame dv \
		mpg123 libdv libnice json-glib libsoup openssl libsrtp \
		libde265 openh264 aom webp libsndfile srt curl
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
	git checkout 1.24.9 || git checkout main
	cd ..
else
	echo "GStreamer repository already exists, skipping clone"
fi

# Step 3: Create Meson native file for Qt private headers (auto-detect + hardcoded versioned private paths)
echo ""
echo "Step 3: Creating Meson native file for Qt6 private headers (auto-detect + hardcoded versioned private paths)..."

detect_qt_version_dir() {
	local module_dir
	module_dir="$1"
	if [ -d "${module_dir}" ]; then
		ls -1 "${module_dir}" 2>/dev/null | grep -E '^[0-9]+\.[0-9]+(\.[0-9]+)?$' \
			| sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1
	fi
}

# Determine Qt version dir present in include layout (e.g., 6.5.6)
QT_VER_DIR=$(detect_qt_version_dir "${QT_PATH}/include/QtCore")
if [ -z "${QT_VER_DIR}" ]; then
	QT_VER_DIR=$(detect_qt_version_dir "${QT_PATH}/include/QtGui")
fi
echo "Detected Qt include version dir: ${QT_VER_DIR}"

# Build include list
qt_extra_includes=()
for p in "${QT_PATH}/include/QtGui" "${QT_PATH}/include/QtCore" "${QT_PATH}/include/QtQml" "${QT_PATH}/include/QtQuick"; do
	[ -d "${p}" ] && qt_extra_includes+=("${p}")
done

if [ -n "${QT_VER_DIR}" ]; then
	for mod in QtGui QtCore QtQml QtQuick; do
		cand_priv="${QT_PATH}/include/${mod}/${QT_VER_DIR}/${mod}/private"
		cand_mod="${QT_PATH}/include/${mod}/${QT_VER_DIR}/${mod}"
		cand_ver_root="${QT_PATH}/include/${mod}/${QT_VER_DIR}"
		[ -d "${cand_priv}" ] && qt_extra_includes+=("${cand_priv}")
		[ -d "${cand_mod}" ] && qt_extra_includes+=("${cand_mod}")
		[ -d "${cand_ver_root}" ] && qt_extra_includes+=("${cand_ver_root}")
	done
fi

# Also include directories containing commonly needed private headers, if present
QRHI_DIR=$(dirname "$(find "${QT_PATH}/include/QtGui" -type f -name 'qrhi_p.h' 2>/dev/null | head -n 1)")
if [ -n "${QRHI_DIR}" ] && [ -d "${QRHI_DIR}" ]; then
	qt_extra_includes+=("$(dirname "${QRHI_DIR}")")
	qt_extra_includes+=("$(dirname "$(dirname "${QRHI_DIR}")")")
fi
QGLOBAL_DIR=$(dirname "$(find "${QT_PATH}/include/QtCore" -type f -name 'qglobal_p.h' 2>/dev/null | head -n 1)")
if [ -n "${QGLOBAL_DIR}" ] && [ -d "${QGLOBAL_DIR}" ]; then
	qt_extra_includes+=("$(dirname "${QGLOBAL_DIR}")")
	qt_extra_includes+=("$(dirname "$(dirname "${QGLOBAL_DIR}")")")
fi

{
	echo "[built-in options]"
	echo -n "cpp_args = ["
	first=true
	for p in "${qt_extra_includes[@]}"; do
		abs_p=$(cd "${p}" 2>/dev/null && pwd || echo "${p}")
		if [ "${first}" = true ]; then first=false; else echo -n ", "; fi
		printf "'"; printf -- "-I%s" "${abs_p}"; printf "'"
	done
	echo "]"
} > meson-native.ini

# Also prepare a space-separated form for -Dcpp_args (Meson CLI)
CPP_EXTRA=""
for p in "${qt_extra_includes[@]}"; do
	abs_p=$(cd "${p}" 2>/dev/null && pwd || echo "${p}")
	CPP_EXTRA+=" -I${abs_p}"
done
# Ensure gettext headers are on the include path for intl detection
CPP_EXTRA+=" -I/opt/homebrew/opt/gettext/include"

echo "Meson native file created: meson-native.ini"

# Step 3b: Install Qt6 pkg-config files from repository templates
echo ""
echo "Step 3b: Installing Qt6 pkg-config files (Core/Gui/Qml/Quick)..."
QT_VER_PKG="${QT_VER_DIR:-6.5.6}"
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
# Also install Qt6Network.pc for QML's network dependency
if [ -f "${BUILD_DIR}/pkgconfig/Qt6Network.pc" ]; then
    sed -e "s#@QT_PREFIX@#${QT_PATH}#g" -e "s#@QT_VERSION@#${QT_VER_PKG}#g" \
        "${BUILD_DIR}/pkgconfig/Qt6Network.pc" > "${QT_PCDIR}/Qt6Network.pc"
fi
export PKG_CONFIG_PATH="${QT_PCDIR}:/opt/homebrew/opt/gettext/lib/pkgconfig:${PKG_CONFIG_PATH}"
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
export PATH="${QT_PATH}/bin:${QT_PATH}/libexec:/opt/homebrew/opt/gettext/bin:/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/flex/bin:${PATH}"
export CMAKE_PREFIX_PATH="${QT_PATH}"
export Qt6_DIR="${QT_PATH}/lib/cmake/Qt6"
export QT_PLUGIN_PATH="${QT_PATH}/plugins:${QT_PLUGIN_PATH}"
export QML2_IMPORT_PATH="${QT_PATH}/qml:${QML2_IMPORT_PATH}"
export LDFLAGS="-L/opt/homebrew/opt/gettext/lib ${LDFLAGS}"
export CPPFLAGS="-I/opt/homebrew/opt/gettext/include ${CPPFLAGS}"
export QT_HOST_BINS="${QT_PATH}/bin"
export QMAKE="${QT_PATH}/bin/qmake6"
export QSB="${QT_PATH}/bin/qsb"
export PKG_CONFIG_PATH="/opt/homebrew/opt/gettext/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Ensure lrelease tool is available (Qt Linguist Tools may be missing in static builds)
if [ ! -x "${QT_PATH}/libexec/lrelease" ] && [ ! -x "${QT_PATH}/bin/lrelease" ]; then
    echo "Creating shim for missing Qt tool: lrelease"
    mkdir -p "${QT_PATH}/libexec"
    cat > "${QT_PATH}/libexec/lrelease" <<'EOF'
#!/bin/sh
case "$1" in
    -v|--version|-version)
        echo "lrelease version 6.5.6";
        exit 0;
        ;;
esac
# No-op shim; qml6 plugin build does not use lrelease.
exit 0
EOF
    chmod +x "${QT_PATH}/libexec/lrelease"
    # Provide alternative names often probed by Meson
    ln -sf "${QT_PATH}/libexec/lrelease" "${QT_PATH}/libexec/lrelease6" 2>/dev/null || true
    ln -sf "${QT_PATH}/libexec/lrelease" "${QT_PATH}/libexec/lrelease-qt6" 2>/dev/null || true
fi

# Ensure Qt private headers are found early by the compiler
if [ -n "${QT_VER_DIR}" ]; then
	export CFLAGS="-I${QT_PATH}/include/QtGui/${QT_VER_DIR}/QtGui -I${QT_PATH}/include/QtCore/${QT_VER_DIR} ${CFLAGS}"
	export CXXFLAGS="-I${QT_PATH}/include/QtGui/${QT_VER_DIR}/QtGui -I${QT_PATH}/include/QtCore/${QT_VER_DIR} ${CXXFLAGS}"
fi

# Configure with Meson (STATIC build)

meson setup builddir \
	--native-file ../meson-native.ini \
	--prefix="${INSTALL_PREFIX}" \
	--buildtype=release \
	-Dcmake_prefix_path="${QT_PATH}" \
	-Dc_args="${CPP_EXTRA}" \
	-Dcpp_args="${CPP_EXTRA}" \
	-Ddefault_library=static \
	-Dgst-full-target-type=static_library \
	-Dgpl=enabled \
	-Dqt5=disabled \
	-Dqt6=enabled \
	-Dgst-plugins-good:qt6=enabled \
	-Dgst-plugins-bad:svtav1=disabled \
	-Dgst-plugins-bad:x265=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Ddoc=disabled \
    -Dnls=disabled \
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
else
	echo "ERROR: libgstreamer-full-1.0.a not found."
	(ls -la "${INSTALL_PREFIX}/lib" || true)
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