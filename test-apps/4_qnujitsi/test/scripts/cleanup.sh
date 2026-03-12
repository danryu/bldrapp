#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "qnujitsi Test Infrastructure Cleanup"
echo "========================================"
echo ""

# Get directories
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Step 1: Stopping gst-meet participants..."
CONTAINERS=$(docker ps -a --filter "name=gst-meet-bot" -q)
if [ -n "$CONTAINERS" ]; then
    docker stop $CONTAINERS 2>/dev/null || true
    docker rm $CONTAINERS 2>/dev/null || true
    echo -e "${GREEN}✓${NC} gst-meet containers removed"
else
    echo "  No gst-meet containers found"
fi

echo ""
echo "Step 2: Stopping Jitsi Meet stack..."
cd "$TEST_ROOT/docker"
if [ -f "docker-compose.yml" ]; then
    docker-compose down -v
    echo -e "${GREEN}✓${NC} Jitsi Meet stack stopped"
else
    echo -e "${YELLOW}⚠${NC} docker-compose.yml not found"
fi

echo ""
echo "Step 3: Removing Docker volumes (optional)..."
read -p "Remove Jitsi configuration volumes? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    cd "$TEST_ROOT/docker"
    rm -rf config/ web/ transcripts/
    echo -e "${GREEN}✓${NC} Volumes removed"
else
    echo "  Volumes preserved"
fi

echo ""
echo "Step 4: Removing test artifacts (optional)..."
read -p "Remove test build directory (legacy) and logs? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$TEST_ROOT/spix/build"
    rm -f "$TEST_ROOT/spix/qnujitsi_spix.log" 2>/dev/null || true
    echo -e "${GREEN}✓${NC} Test build artifacts removed"
else
    echo "  Test build artifacts preserved"
fi

echo ""
echo "Step 5: Removing dependencies (optional)..."
read -p "Remove local dependencies (AnyRPC, Spix)? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$TEST_ROOT/deps"
    echo -e "${GREEN}✓${NC} Local dependencies removed"
else
    echo "  Local dependencies preserved"
fi

echo ""
echo "========================================"
echo -e "${GREEN}Cleanup complete!${NC}"
echo "========================================"
echo ""
echo "To rebuild test environment, run:"
echo "  cd $TEST_ROOT/scripts && ./setup_test_env.sh"
echo ""
