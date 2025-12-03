#!/bin/bash
set -e

# GStreamer Cross-Platform Static Build Script with qml6glsink plugin
# Requires Statically-built Qt6 installation to build against
# Produces static libraries for GStreamer, qml6glsink plugin and gstjitsimeet(jitsibin) plugin
# Supports: macOS (and maybe therefore iOS), Linux, Windows (MSVC via MSYS2)

echo "=================================================="
echo "GStreamer Build Script with qml6glsink Support"
echo "=================================================="

# Platform Detection
if [[ "$OSTYPE" == "darwin"* ]]; then
	PLATFORM="macos"
	ARCH=$(uname -m)  # arm64 or x86_64
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
	PLATFORM="linux"
	ARCH=$(uname -m)
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
	PLATFORM="windows"
	ARCH="x86_64"  # Assume 64-bit Windows
else
	echo "ERROR: Unsupported platform: $OSTYPE"
	exit 1
fi

echo "Detected platform: ${PLATFORM} (${ARCH})"

# Configuration
BUILD_DIR="$(pwd)"
INSTALL_PREFIX="${BUILD_DIR}/install"

# Qt6 Configuration (STATIC QT)
# Platform-specific Qt paths - override via environment variable QT_PATH if needed
if [ -z "${QT_PATH}" ]; then
	case "${PLATFORM}" in
		macos)
			QT_PATH="/Users/dan/qt6-static-build"
			;;
		linux)
			QT_PATH="/opt/qt6-static"
			;;
		windows)
			QT_PATH="C:/Qt/6.5.7-static"
			;;
	esac
fi

echo "Build directory: ${BUILD_DIR}"
echo "Install prefix: ${INSTALL_PREFIX}"
echo "Qt6 path: ${QT_PATH}"

# Step 1: (Optional) Install build dependencies
echo ""
echo "Step 1: Checking dependencies..."
INSTALL_DEPS=${INSTALL_DEPS:-false}
if [ "${INSTALL_DEPS}" = true ]; then
	case "${PLATFORM}" in
		macos)
			echo "Installing dependencies via Homebrew..."
			brew install meson ninja pkg-config python3 bison flex nasm
			;;
		linux)
			echo "Installing dependencies via apt (Debian/Ubuntu)..."
			sudo apt-get update
			sudo apt-get install -y meson ninja-build pkg-config python3 bison flex nasm
			;;
		windows)
			echo "Installing dependencies via MSYS2 pacman..."
			echo "NOTE: Ensure you're running in MSYS2 MSYS environment (not MINGW64)"
			echo "NOTE: Using MSYS packages (not mingw-w64) to ensure MSVC compatibility"
			pacman -S --noconfirm --needed \
				meson \
				ninja \
				pkg-config \
				python \
				python-pip \
				bison \
				flex \
				nasm \
				git \
				perl \
				make \
				diffutils \
				patch
			;;
	esac
else
	echo "Skipping dependency installation. Ensure meson, ninja, pkg-config, python3, bison, flex, nasm are installed."
	case "${PLATFORM}" in
		windows)
			echo "For Windows: Install MSYS2 (MSYS environment, not MINGW64) and run:"
			echo "  pacman -S meson ninja pkg-config python python-pip bison flex nasm git perl make diffutils patch"
			;;
	esac
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

# Platform-specific compiler flags and environment
case "${PLATFORM}" in
	macos)
		export MACOSX_DEPLOYMENT_TARGET="13.3"
		export CFLAGS="-mmacosx-version-min=13.3 ${CFLAGS}"
		export CXXFLAGS="-mmacosx-version-min=13.3 ${CXXFLAGS}"
		echo "Set macOS deployment target: ${MACOSX_DEPLOYMENT_TARGET}"
		;;
	linux)
		# Linux-specific flags if needed
		echo "Linux build environment configured"
		;;
	windows)
		echo "Windows MSVC build environment"

		# Step 1: Verify we're running in MSYS2
		echo "Verifying MSYS2 environment..."
		if [ ! -f "/usr/bin/pacman" ]; then
			echo "ERROR: This script must be run in MSYS2 environment"
			echo "Please install MSYS2 from https://www.msys2.org/"
			echo "Then run this script from the MSYS2 MSYS terminal (not Git Bash or MINGW64)"
			exit 1
		fi

		# Check we're in MSYS environment (not MINGW64)
		if [ -n "$MSYSTEM" ] && [ "$MSYSTEM" != "MSYS" ]; then
			echo "WARNING: You're in $MSYSTEM environment. For MSVC builds, use MSYS environment."
			echo "Please run: C:\\msys64\\msys2.exe"
		fi

		echo "✓ Running in MSYS2"

		# Step 2: Check for MSVC compiler
		if ! command -v cl.exe &> /dev/null; then
			echo "MSVC compiler (cl.exe) not found in PATH"
			echo "Attempting to locate and configure Visual Studio..."

			# Try to find Visual Studio installation
			VS_VCVARS=""
			if [ -f "/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build/vcvars64.bat" ]; then
				VS_VCVARS="/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Auxiliary/Build/vcvars64.bat"
				VS_EDITION="Enterprise"
			elif [ -f "/c/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build/vcvars64.bat" ]; then
				VS_VCVARS="/c/Program Files/Microsoft Visual Studio/2022/Professional/VC/Auxiliary/Build/vcvars64.bat"
				VS_EDITION="Professional"
			elif [ -f "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" ]; then
				VS_VCVARS="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat"
				VS_EDITION="Community"
			elif [ -f "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat" ]; then
				VS_VCVARS="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat"
				VS_EDITION="BuildTools"
			fi

			if [ -n "$VS_VCVARS" ]; then
				echo "Found Visual Studio 2022 ${VS_EDITION}"
				echo "Setting up MSVC environment variables..."

				# Convert MSYS2 path to Windows path for vcvars64.bat
				VS_VCVARS_WIN=$(cygpath -w "$VS_VCVARS")

				# Source MSVC environment variables
				eval "$(cmd.exe //c "\"${VS_VCVARS_WIN}\"" '&' set | grep -E '^(PATH|INCLUDE|LIB|LIBPATH)=' | sed 's/\\/\\\\/g; s/^/export /; s/=/=\"/; s/$/\"/')"

				# Verify cl.exe is now available
				if command -v cl.exe &> /dev/null; then
					echo "✓ MSVC environment configured successfully"
					echo "  Compiler: $(which cl.exe)"
				else
					echo "ERROR: Failed to configure MSVC environment"
					echo "Please run this script from a Developer Command Prompt, or manually source vcvars64.bat:"
					echo "  eval \"\$(cmd.exe //c '\"${VS_VCVARS_WIN}\"' '&' set | grep -E '^(PATH|INCLUDE|LIB|LIBPATH)=' | sed 's/\\\\/\\\\\\\\/g; s/^/export /; s/=/=\\\"/; s/$/\\\"/')\""
					exit 1
				fi
			else
				echo "ERROR: Visual Studio 2022 not found"
				echo "Please install Visual Studio 2022 or Build Tools from:"
				echo "  https://visualstudio.microsoft.com/downloads/"
				echo "Make sure to install 'Desktop development with C++' workload"
				exit 1
			fi
		else
			echo "✓ Found MSVC compiler: $(which cl.exe)"
		fi

		# Step 3: Verify nmake is available
		if ! command -v nmake &> /dev/null; then
			echo "ERROR: nmake not found. It should be in the same directory as cl.exe"
			exit 1
		fi
		echo "✓ Found nmake: $(which nmake)"

		;;
esac

##############################################################################################################################
# Build OpenSSL separately before Meson configure
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

	# Platform-specific OpenSSL configuration
	case "${PLATFORM}" in
		macos)
			# macOS: darwin64-arm64-cc for Apple Silicon, darwin64-x86_64-cc for Intel
			if [ "${ARCH}" = "arm64" ]; then
				OPENSSL_TARGET="darwin64-arm64-cc"
			else
				OPENSSL_TARGET="darwin64-x86_64-cc"
			fi

			./Configure ${OPENSSL_TARGET} \
				--prefix="${OPENSSL_INSTALL}" \
				--openssldir=/etc/ssl \
				no-shared \
				no-tests \
				-mmacosx-version-min="${MACOSX_DEPLOYMENT_TARGET:-13.3}"

			echo "Building OpenSSL with make..."
			make -j$(sysctl -n hw.ncpu)
			make install_sw
			;;

		linux)
			# Linux: linux-x86_64 or linux-aarch64
			if [ "${ARCH}" = "aarch64" ]; then
				OPENSSL_TARGET="linux-aarch64"
			else
				OPENSSL_TARGET="linux-x86_64"
			fi

			./Configure ${OPENSSL_TARGET} \
				--prefix="${OPENSSL_INSTALL}" \
				--openssldir=/etc/ssl \
				no-shared \
				no-tests

			echo "Building OpenSSL with make..."
			make -j$(nproc)
			make install_sw
			;;

		windows)
			# Windows: Use MSVC (VC-WIN64A)
			# OpenSSL on Windows uses system certificate store automatically via CAPI engine
			echo "Configuring OpenSSL for Windows with MSVC..."

			perl Configure VC-WIN64A \
				--prefix="${OPENSSL_INSTALL}" \
				no-shared \
				no-tests \
				no-asm

			echo "Building OpenSSL with nmake..."
			nmake
			nmake install_sw
			;;
	esac

	cd "${BUILD_DIR}/gstreamer"
	echo "OpenSSL build complete!"
else
	echo "OpenSSL already built at ${OPENSSL_INSTALL}"
fi

# Expose OpenSSL via PKG_CONFIG_PATH
export PKG_CONFIG_PATH="${OPENSSL_INSTALL}/lib/pkgconfig:${PKG_CONFIG_PATH}"
echo "Added OpenSSL to PKG_CONFIG_PATH: ${OPENSSL_INSTALL}/lib/pkgconfig"

##############################################################################################################################
####################################################################################################
# # Build libvpx separately for VP8/VP9 support
# ##############################################################################################################################
# VPX_INSTALL="${BUILD_DIR}/vpx-install"
# echo ""
# echo "Step 3c: Building libvpx from source..."
# if [ ! -d "${VPX_INSTALL}" ]; then
# 	cd "${BUILD_DIR}"
# 	if [ ! -d "libvpx-src" ]; then
# 		echo "Cloning libvpx v1.14.1..."
# 		git clone --depth 1 --branch v1.14.1 https://chromium.googlesource.com/webm/libvpx libvpx-src
# 	fi
# 	cd libvpx-src
	
# 	echo "Configuring libvpx..."
# 	./configure \
# 		--prefix="${VPX_INSTALL}" \
# 		--disable-shared \
# 		--enable-static \
# 		--disable-examples \
# 		--disable-tools \
# 		--disable-docs \
# 		--disable-unit-tests \
# 		--enable-vp8 \
# 		--enable-vp9 \
# 		--enable-vp9-highbitdepth \
# 		--as=yasm
	
# 	echo "Building libvpx..."
# 	make -j$(sysctl -n hw.ncpu)
	
# 	echo "Installing libvpx to ${VPX_INSTALL}..."
# 	make install
	
# 	cd "${BUILD_DIR}/gstreamer"
# 	echo "libvpx build complete!"
# else
# 	echo "libvpx already built at ${VPX_INSTALL}"
# fi

# # Expose libvpx via PKG_CONFIG_PATH
# export PKG_CONFIG_PATH="${VPX_INSTALL}/lib/pkgconfig:${PKG_CONFIG_PATH}"
# echo "Added libvpx to PKG_CONFIG_PATH: ${VPX_INSTALL}/lib/pkgconfig"
# ############################################################
# ############################################################################################

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

	# Determine CPU count for parallel builds
	case "${PLATFORM}" in
		macos)
			NUM_CPUS=$(sysctl -n hw.ncpu)
			;;
		linux)
			NUM_CPUS=$(nproc)
			;;
		windows)
			NUM_CPUS=${NUMBER_OF_PROCESSORS:-4}
			;;
	esac

	echo "Configuring libwebsockets with CMake..."

	# Platform-specific OpenSSL library names
	if [ "${PLATFORM}" = "windows" ]; then
		# Windows MSVC uses .lib extension
		OPENSSL_CRYPTO_LIB="${OPENSSL_INSTALL}/lib/libcrypto.lib"
		OPENSSL_SSL_LIB="${OPENSSL_INSTALL}/lib/libssl.lib"
		LWS_UNIX_SOCK_OPTION=""
	else
		# Unix platforms use .a extension
		OPENSSL_CRYPTO_LIB="${OPENSSL_INSTALL}/lib/libcrypto.a"
		OPENSSL_SSL_LIB="${OPENSSL_INSTALL}/lib/libssl.a"
		LWS_UNIX_SOCK_OPTION="-DLWS_UNIX_SOCK=ON"
	fi

	# Build CMake command with platform-specific options
	CMAKE_CMD=(
		cmake -B build
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5
		-DCMAKE_BUILD_TYPE=Release
		-DCMAKE_INSTALL_PREFIX="${LWS_INSTALL}"
		-DOPENSSL_ROOT_DIR="${OPENSSL_INSTALL}"
		-DOPENSSL_INCLUDE_DIR="${OPENSSL_INSTALL}/include"
		-DOPENSSL_CRYPTO_LIBRARY="${OPENSSL_CRYPTO_LIB}"
		-DOPENSSL_SSL_LIBRARY="${OPENSSL_SSL_LIB}"
		-DLWS_WITH_SHARED=OFF
		-DLWS_WITH_STATIC=ON
		-DLWS_WITHOUT_TESTAPPS=ON
		-DLWS_WITHOUT_TEST_SERVER=ON
		-DLWS_WITHOUT_TEST_PING=ON
		-DLWS_WITHOUT_TEST_CLIENT=ON
		-DLWS_WITH_SSL=ON
		-DLWS_OPENSSL_SUPPORT=ON
		-DLWS_WITH_ZLIB=OFF
		-DLWS_WITHOUT_EXTENSIONS=ON
		-DLWS_IPV6=ON
		-DLWS_WITH_LIBUV=OFF
		-DLWS_WITH_LIBEVENT=OFF
		-DLWS_WITH_GLIB=OFF
		-DLWS_WITH_HTTP2=OFF
		-DLWS_WITH_MINIMAL_EXAMPLES=OFF
		-DLWS_LINK_TESTAPPS_DYNAMIC=OFF
	)

	# Add macOS-specific options
	if [ "${PLATFORM}" = "macos" ]; then
		CMAKE_CMD+=(-DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.3}")
	fi

	# Add Unix socket support for non-Windows platforms
	if [ -n "${LWS_UNIX_SOCK_OPTION}" ]; then
		CMAKE_CMD+=(${LWS_UNIX_SOCK_OPTION})
	fi

	"${CMAKE_CMD[@]}"

	echo "Building libwebsockets..."
	cmake --build build --config Release -j${NUM_CPUS}

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
# Setup gstjitsimeet plugin and its dependencies
# Steps 1-3 from gstjitsimeet/build_gstjitsimeet.sh:
# 1. Clone gstjitsimeet repository
# 2. Clone and setup submod utility (for submodule management)
# 3. Clone and build coop dependency
# 4. Initialize gstjitsimeet submodules
##############################################################################################################################
echo ""
echo "Step 3f: Setting up gstjitsimeet plugin and dependencies..."

GSTJ_DIR="${BUILD_DIR}/gstjitsimeet"
GSTJ_DEPS="${GSTJ_DIR}/deps"
GSTJ_COOP_INSTALL="${GSTJ_DEPS}/coop-install"

# Step 1: Clone gstjitsimeet repository
if [ ! -d "${GSTJ_DIR}" ]; then
	echo "Cloning gstjitsimeet repository..."
	cd "${BUILD_DIR}"
	git clone -b kdev https://github.com/danryu/gstjitsimeet.git
	cd gstreamer
else
	echo "gstjitsimeet repository already exists at ${GSTJ_DIR}"
fi

# Step 2: Clone and setup submod utility
echo "Setting up submod utility..."
mkdir -p "${GSTJ_DEPS}"
cd "${GSTJ_DEPS}"

if [ ! -d "submod" ]; then
	echo "Cloning danryu/submod (kdev branch)..."
	git clone -b kdev https://github.com/danryu/submod.git
else
	echo "submod directory already exists"
	cd submod
	git fetch origin || true
	git checkout kdev || true
	git pull || true
	cd ..
fi

echo "submod utility ready at: ${GSTJ_DEPS}/submod"

# Step 3: Clone and build coop dependency
echo "Building coop dependency..."

if [ ! -d "coop" ]; then
	echo "Cloning danryu/coop (kdev branch)..."
	git clone -b kdev https://github.com/danryu/coop.git
else
	echo "coop directory already exists"
	cd coop
	git fetch origin || true
	git checkout kdev || true
	git pull || true
	cd ..
fi

# Build coop if not already built
if [ ! -d "${GSTJ_COOP_INSTALL}" ]; then
	cd coop
	echo "Building coop (kdev branch)..."
	if [ -d "build" ]; then
		rm -rf build
	fi

	# Determine CPU count for parallel builds
	case "${PLATFORM}" in
		macos)
			NUM_CPUS=$(sysctl -n hw.ncpu)
			;;
		linux)
			NUM_CPUS=$(nproc)
			;;
		windows)
			NUM_CPUS=${NUMBER_OF_PROCESSORS:-4}
			;;
	esac

	meson setup build --prefix="${GSTJ_COOP_INSTALL}" --buildtype=release --default-library=static
	ninja -C build -j${NUM_CPUS}
	ninja -C build install

	echo "coop built and installed to: ${GSTJ_COOP_INSTALL}"
	cd "${GSTJ_DEPS}"
else
	echo "coop already built at ${GSTJ_COOP_INSTALL}"
fi

# Step 4: Initialize gstjitsimeet submodules with submod
echo "Initializing gstjitsimeet submodules..."
cd "${GSTJ_DIR}"
"${GSTJ_DEPS}/submod/submod" clone

echo "gstjitsimeet submodules initialized"

# Return to gstreamer directory for rest of build
cd "${BUILD_DIR}/gstreamer"

echo "gstjitsimeet setup complete!"
##############################################################################################################################


##############################################################################################################################

# Ensure our local wraps are installed for custom subprojects
if [ -f "${BUILD_DIR}/wraps/gstjitsimeet.wrap" ]; then
	mkdir -p "subprojects"
	cp -f "${BUILD_DIR}/wraps/gstjitsimeet.wrap" "subprojects/gstjitsimeet.wrap"
fi

# Link gstjitsimeet directory into subprojects to avoid any VCS/network usage
if [ -d "${GSTJ_DIR}" ] && [ ! -d "subprojects/gstjitsimeet" ]; then
	ln -s "${GSTJ_DIR}" "subprojects/gstjitsimeet"
fi

# Expose coop pkg-config path for gstjitsimeet build
if [ -d "${GSTJ_COOP_INSTALL}/lib/pkgconfig" ]; then
	export PKG_CONFIG_PATH="${GSTJ_COOP_INSTALL}/lib/pkgconfig:${PKG_CONFIG_PATH}"
	echo "Added coop to PKG_CONFIG_PATH: ${GSTJ_COOP_INSTALL}/lib/pkgconfig"
fi
##############################################################################################################################


# Configure with Meson (STATIC build)
# Enable gst-full (monolithic static library)
# Use wildcard for plugins to include all enabled plugins

echo "Configuring GStreamer with Meson..."

# Build platform-specific plugin options
PLATFORM_PLUGINS=()
case "${PLATFORM}" in
	macos)
		echo "Enabling macOS-specific plugins: osxvideo, osxaudio, applemedia..."
		PLATFORM_PLUGINS+=(
			"-Dgst-plugins-good:osxvideo=enabled"
			"-Dgst-plugins-good:osxaudio=enabled"
			"-Dgst-plugins-bad:applemedia=enabled"
		)
		;;
	linux)
		echo "Enabling Linux-specific plugins: ximagesink, pulseaudio, v4l2..."
		PLATFORM_PLUGINS+=(
			"-Dgst-plugins-good:pulse=enabled"
			"-Dgst-plugins-good:v4l2=enabled"
			# for AMD/Intel VA-API support
			"-Dgst-plugins-bad:va=enabled"
			# for NVIDIA NVENC support
			"-Dgst-plugins-bad:nvcodec=enabled"    
			# Software encoder (x264) - requires gst-plugins-ugly
			"-Dgstreamer-1.0:ugly=enabled"
			"-Dgst-plugins-ugly:x264=enabled"
			# Software decoder (avdec_h264) - requires gst-libav
			"-Dgstreamer-1.0:libav=enabled"

		)
		;;
	windows)
		echo "Enabling Windows-specific plugins: wasapi, winks, nvcodec, amfcodec, msdk, x264, libav..."
		PLATFORM_PLUGINS+=(
			# Audio input/output (WASAPI)
			"-Dgst-plugins-bad:wasapi=enabled"
			# Video capture (Windows kernel streaming)
			"-Dgst-plugins-bad:winks=enabled"
			# Hardware encoders/decoders
			"-Dgst-plugins-bad:nvcodec=enabled"    # NVIDIA
			"-Dgst-plugins-bad:amfcodec=enabled"   # AMD
			"-Dgst-plugins-bad:msdk=enabled"       # Intel Media SDK
			# Software encoder (x264) - requires gst-plugins-ugly
			"-Dgstreamer-1.0:ugly=enabled"
			"-Dgst-plugins-ugly:x264=enabled"
			# Software decoder (avdec_h264) - requires gst-libav
			"-Dgstreamer-1.0:libav=enabled"
		)
		;;
esac

# Common Meson options
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
	-Dgst-plugins-good:qt6=enabled \
	-Dgst-plugins-bad:dtls=enabled \
	-Dgst-plugins-bad:srtp=enabled \
	-Dgst-plugins-bad:videoparsers=enabled \
	-Dlibnice=enabled \
	-Dlibnice:crypto-library=openssl \
	-Dlibnice:gstreamer=enabled \
	-Dlibnice:tests=disabled \
	-Dlibnice:examples=disabled \
	-Dlibnice:introspection=disabled \
	-Dqt6=enabled \
	"${PLATFORM_PLUGINS[@]}"

# REMOVED !!!!
	# -Drs=enabled \
	# -Dgst-plugins-rs:rtp=enabled \
	# -Dgst-plugins-rs:dav1d=enabled \
	# -Dgst-plugins-rs:rav1e=enabled \
	# -Dgst-plugins-bad:aom=enabled \
	# -Dgst-plugins-bad:svtav1=enabled \
	# -Dgst-plugins-bad:openh264=enabled \
	# -Dgstreamer-1.0:libav=enabled \
	# -Dgst-plugins-good:vpx=enabled \



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

echo "Looking for libgstreamer-full-1.0 static library under ${INSTALL_PREFIX}/lib..."

# Platform-specific static library extension
if [ "${PLATFORM}" = "windows" ]; then
	GST_FULL_LIB="${INSTALL_PREFIX}/lib/libgstreamer-full-1.0.lib"
	LIB_EXT="lib"
else
	GST_FULL_LIB="${INSTALL_PREFIX}/lib/libgstreamer-full-1.0.a"
	LIB_EXT="a"
fi

if [ -f "${GST_FULL_LIB}" ]; then
    echo "Found static lib: ${GST_FULL_LIB}"
    echo "Size: $(ls -lh "${GST_FULL_LIB}" | awk '{print $5}')"
else
    echo "ERROR: libgstreamer-full-1.0.${LIB_EXT} not found."
    echo "Available libraries:"
    (ls -lh "${INSTALL_PREFIX}/lib"/*.${LIB_EXT} 2>/dev/null || true)
    (ls -lh "${INSTALL_PREFIX}/lib"/ 2>/dev/null || true)
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
if [ "${PLATFORM}" = "windows" ]; then
	echo "  ls -la ${INSTALL_PREFIX}/lib/libgstreamer-full-1.0.lib"
else
	echo "  ls -la ${INSTALL_PREFIX}/lib/libgstreamer-full-1.0.a"
fi
echo "  find ${INSTALL_PREFIX} -name 'libgstqml6*.*'"
echo ""
echo "Key components installed:"
if [ "${PLATFORM}" = "windows" ]; then
	echo "  - GStreamer core libraries (static libgstreamer-full-1.0.lib)"
else
	echo "  - GStreamer core libraries (static libgstreamer-full-1.0.a)"
fi
echo "  - Qt6 QML plugin (qml6glsink)"
echo "  - Platform-specific plugins (${PLATFORM})"
echo "  - Tools (gst-inspect, gst-launch, etc.)"
echo ""