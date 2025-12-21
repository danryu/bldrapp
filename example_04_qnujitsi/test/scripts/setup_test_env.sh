#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "qnujitsi Test Environment Setup"
echo "========================================"
echo ""

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check dependencies
echo "Checking dependencies..."

if ! command -v docker &> /dev/null; then
    echo -e "${RED}ERROR: Docker is not installed${NC}"
    echo "Install Docker from: https://docs.docker.com/get-docker/"
    exit 1
fi
echo -e "${GREEN}✓${NC} Docker found"

if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo -e "${RED}ERROR: Docker Compose is not installed${NC}"
    echo "Install Docker Compose or use 'docker compose' plugin"
    exit 1
fi
echo -e "${GREEN}✓${NC} Docker Compose found"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: CMake is not installed${NC}"
    echo "Install CMake from: https://cmake.org/download/"
    exit 1
fi
echo -e "${GREEN}✓${NC} CMake found"

# Check if Qt6 path exists
QT6_PATH="/Users/dan/qt6-static-build"
if [ ! -d "$QT6_PATH" ]; then
    echo -e "${YELLOW}WARNING: Qt6 path not found at $QT6_PATH${NC}"
    echo "Update QT_INSTALL_PREFIX in test/spix/CMakeLists.txt if needed"
fi

echo ""
echo "Step 1: Building gst-meet Docker image..."
cd "$TEST_ROOT/gst-meet"
docker build -t qnujitsi-gst-meet:latest .
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} gst-meet image built successfully"
else
    echo -e "${RED}✗${NC} Failed to build gst-meet image"
    exit 1
fi

echo ""
echo "Step 2: Pulling Jitsi Meet Docker images..."
cd "$TEST_ROOT/docker"
docker-compose pull
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Jitsi images pulled successfully"
else
    echo -e "${YELLOW}⚠${NC} Some images may have failed to pull (continuing...)"
fi

echo ""
echo "Step 3: Building local dependencies (AnyRPC and Spix)..."

# Create local installation directory
DEPS_DIR="$TEST_ROOT/deps"
DEPS_INSTALL="$DEPS_DIR/install"
mkdir -p "$DEPS_DIR"
mkdir -p "$DEPS_INSTALL"

# Get number of CPU cores
NCPU=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Build AnyRPC
echo ""
echo "  Building AnyRPC..."
cd "$DEPS_DIR"
if [ ! -d "anyrpc" ]; then
    git clone https://github.com/sgieseking/anyrpc.git
fi
cd anyrpc
mkdir -p build
cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX="$DEPS_INSTALL" \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_WITH_LOG4CPLUS=OFF \
    -DBUILD_PROTOCOL_MESSAGEPACK=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
if [ $? -ne 0 ]; then
    echo -e "${RED}✗${NC} AnyRPC CMake configuration failed"
    exit 1
fi

cmake --build . -j${NCPU}
if [ $? -ne 0 ]; then
    echo -e "${RED}✗${NC} AnyRPC build failed"
    exit 1
fi

cmake --install .
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} AnyRPC built and installed to $DEPS_INSTALL"
else
    echo -e "${RED}✗${NC} AnyRPC installation failed"
    exit 1
fi

# Build Spix
echo ""
echo "  Building Spix..."
cd "$DEPS_DIR"
if [ ! -d "spix" ]; then
    git clone https://github.com/faaxm/spix.git
fi
cd spix
mkdir -p build
cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX="$DEPS_INSTALL" \
    -DCMAKE_PREFIX_PATH="$QT6_PATH;$DEPS_INSTALL" \
    -DSPIX_BUILD_EXAMPLES=OFF \
    -DSPIX_BUILD_TESTS=OFF
if [ $? -ne 0 ]; then
    echo -e "${RED}✗${NC} Spix CMake configuration failed"
    exit 1
fi

cmake --build . -j${NCPU}
if [ $? -ne 0 ]; then
    echo -e "${RED}✗${NC} Spix build failed"
    exit 1
fi

cmake --install .
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Spix built and installed to $DEPS_INSTALL"
else
    echo -e "${RED}✗${NC} Spix installation failed"
    exit 1
fi

echo ""
echo "Step 4: Building qnujitsi tests..."
echo "Step 4: Spix RPC test scripts ready (Python)"
echo ""
echo "IMPORTANT: qnujitsi must be built with Spix enabled so it starts the RPC server."
echo "Example:"
echo "  cd \"$TEST_ROOT/../build\""
echo "  cmake .. -DCMAKE_PREFIX_PATH=\"$QT6_PATH;$DEPS_INSTALL;$DEPS_INSTALL/lib\" -DQNUJITSI_ENABLE_SPIX=ON"
echo "  make -j${NCPU}"

echo ""
echo "========================================"
echo -e "${GREEN}Test environment setup complete!${NC}"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. Run tests: cd $TEST_ROOT/scripts && ./run_tests.sh"
echo "2. Or manually start Jitsi: cd $TEST_ROOT/docker && docker-compose up -d"
echo ""
