#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================"
echo "qnujitsi Automated Test Suite"
echo "========================================"
echo ""

# Get directories
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT="$(cd "$TEST_ROOT/.." && pwd)"

# Configuration
JITSI_HTTP_PORT="${JITSI_HTTP_PORT:-8000}"
JITSI_HTTPS_PORT="${JITSI_HTTPS_PORT:-30443}"
JITSI_SERVER="localhost:${JITSI_HTTPS_PORT}"
USE_TLS=1
JITSI_ROOM="${JITSI_ROOM:-testroom}"
PARTICIPANT_NAME="TestBot1"
PARTICIPANT_COUNT=1  # Number of gst-meet bots to launch

# Test result
TEST_RESULT=0

# Cleanup function
cleanup() {
    echo ""
    echo -e "${BLUE}Cleaning up test infrastructure...${NC}"

    # Stop gst-meet participants
    for i in $(seq 1 $PARTICIPANT_COUNT); do
        docker stop gst-meet-bot$i 2>/dev/null || true
        docker rm gst-meet-bot$i 2>/dev/null || true
    done

    # Stop Jitsi Meet stack
    cd "$TEST_ROOT/docker"
    docker-compose down

    echo -e "${GREEN}Cleanup complete${NC}"
}

# Trap cleanup on exit
trap cleanup EXIT

echo "Step 1: Starting Jitsi Meet server..."
cd "$TEST_ROOT/docker"

# If the desired HTTP port is occupied, pick the next free one.
if command -v lsof &> /dev/null; then
    for try_port in $(seq "$JITSI_HTTP_PORT" $((JITSI_HTTP_PORT+20))); do
        if lsof -nP -iTCP:"$try_port" -sTCP:LISTEN >/dev/null 2>&1; then
            continue
        fi
        JITSI_HTTP_PORT="$try_port"
        break
    done
fi
export JITSI_HTTP_PORT
export JITSI_HTTPS_PORT
JITSI_SERVER="localhost:${JITSI_HTTPS_PORT}"

docker-compose up -d

echo "Waiting for services to initialize (5 seconds)..."
sleep 5

# echo ""
# echo "Step 2: Waiting for Jitsi Meet to be ready..."
# RETRIES=30
# for i in $(seq 1 $RETRIES); do
#     if curl -s -o /dev/null -w "%{http_code}" "https://localhost:${JITSI_HTTPS_PORT}" | grep -q "200"; then
#         echo -e "${GREEN}✓${NC} Jitsi Meet is ready!"
#         break
#     fi

#     if [ $i -eq $RETRIES ]; then
#         echo -e "${RED}✗${NC} Jitsi Meet did not become ready in time"
#         echo "Check logs: cd $TEST_ROOT/docker && docker-compose logs"
#         exit 1
#     fi

#     echo "  Attempt $i/$RETRIES..."
#     sleep 1
# done

echo ""
echo "Step 3: Starting gst-meet participant simulators..."

# Launch participant(s)
for i in $(seq 1 $PARTICIPANT_COUNT); do
    BOT_NAME="TestBot$i"
    CONTAINER_NAME="gst-meet-bot$i"

    # Ensure stale containers don't block reruns
    docker rm -f "$CONTAINER_NAME" 2>/dev/null || true

    # Different patterns for different bots
    if [ $i -eq 1 ]; then
        VIDEO_PATTERN="smpte"
        AUDIO_FREQ="440"
    else
        VIDEO_PATTERN="ball"
        AUDIO_FREQ="880"
    fi

    echo "  Launching $BOT_NAME..."
    docker run -d --name $CONTAINER_NAME --network host \
        -e JITSI_SERVER="$JITSI_SERVER" \
        -e JITSI_ROOM="$JITSI_ROOM" \
        -e PARTICIPANT_NAME="$BOT_NAME" \
        -e VIDEO_PATTERN="$VIDEO_PATTERN" \
        -e AUDIO_FREQ="$AUDIO_FREQ" \
        qnujitsi-gst-meet:latest

    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓${NC} $BOT_NAME started"
    else
        echo -e "  ${RED}✗${NC} Failed to start $BOT_NAME"
    fi
done

echo "Waiting for participants to join (5 seconds)..."
sleep 5

echo ""
echo "========================================="
echo "Diagnostic Checks"
echo "========================================="

# Check gst-meet bot status
echo "Checking gst-meet bot status..."
for i in $(seq 1 $PARTICIPANT_COUNT); do
    CONTAINER_NAME="gst-meet-bot$i"
    if docker logs "$CONTAINER_NAME" 2>&1 | grep -q "Joined MUC"; then
        echo -e "  ${GREEN}✓${NC} $CONTAINER_NAME joined conference successfully"
    else
        echo -e "  ${YELLOW}⚠${NC} $CONTAINER_NAME may not have joined (checking logs...)"
        docker logs "$CONTAINER_NAME" 2>&1 | tail -5
    fi
done

# Check Jitsi web interface
echo ""
echo "Checking Jitsi web interface..."
HTTP_CODE=$(curl -k -s -o /dev/null -w "%{http_code}" "https://localhost:${JITSI_HTTPS_PORT}" || echo "000")
if [ "$HTTP_CODE" = "200" ]; then
    echo -e "  ${GREEN}✓${NC} Jitsi web interface responding (HTTP $HTTP_CODE)"
else
    echo -e "  ${RED}✗${NC} Jitsi web interface not responding properly (HTTP $HTTP_CODE)"
fi

# echo ""
# echo "========================================="
# echo "Manual Verification Available"
# echo "========================================="
# echo "Jitsi Meet URL: https://localhost:${JITSI_HTTPS_PORT}"
# echo "Room: ${JITSI_ROOM}"
# echo ""
# echo "Opening browser for manual verification..."
# echo ""
# echo "What to verify:"
# echo "  1. Browser opens to Jitsi Meet room"
# echo "  2. You should see TestBot1 in the conference"
# echo "  3. TestBot1 should show SMPTE color bars (video)"
# echo "  4. TestBot1 should have audio indicator active"
# echo ""
# echo "If you don't see TestBot1, check the logs above for errors."
# echo ""

# # Open browser (works on macOS, Linux, Windows)
# if command -v open &> /dev/null; then
#     # macOS
#     open "https://localhost:${JITSI_HTTPS_PORT}/${JITSI_ROOM}" &
# elif command -v xdg-open &> /dev/null; then
#     # Linux
#     xdg-open "https://localhost:${JITSI_HTTPS_PORT}/${JITSI_ROOM}" &
# elif command -v start &> /dev/null; then
#     # Windows
#     start "https://localhost:${JITSI_HTTPS_PORT}/${JITSI_ROOM}" &
# fi

echo "Press Enter to continue with automated tests (or Ctrl+C to exit)..."
read -r

echo ""
echo "Step 4: Running GUI tests..."
cd "$TEST_ROOT/spix"

# qnujitsi binary (must have Spix RPC server enabled)
QNUJITSI_BIN="$PROJECT_ROOT/build/qnujitsi"
if [ ! -f "$QNUJITSI_BIN" ]; then
    echo -e "${RED}ERROR: qnujitsi binary not found at $QNUJITSI_BIN${NC}"
    echo "Build qnujitsi first:"
    echo "  cd $PROJECT_ROOT/build && cmake .. -DCMAKE_PREFIX_PATH=\"/Users/dan/qt6-static-build;$TEST_ROOT/deps/install;$TEST_ROOT/deps/install/lib\" -DQNUJITSI_ENABLE_SPIX=ON && make"
    exit 1
fi

echo "Using qnujitsi binary: $QNUJITSI_BIN"

# Launch qnujitsi in background (Spix RPC server should start on port 9000)
echo "Launching qnujitsi (Spix RPC server expected on :9000)..."
export QT_LOGGING_RULES="qt.qml.binding.removal.info=false"
$QNUJITSI_BIN > "$TEST_ROOT/spix/qnujitsi_spix.log" 2>&1 &
QNUJITSI_PID=$!

# Ensure we kill qnujitsi if the script exits early
kill_qnujitsi() {
    if kill -0 "$QNUJITSI_PID" 2>/dev/null; then
        kill "$QNUJITSI_PID" 2>/dev/null || true
        wait "$QNUJITSI_PID" 2>/dev/null || true
    fi
}
trap kill_qnujitsi EXIT

echo "Waiting for Spix RPC to be ready..."
RETRIES=50
for i in $(seq 1 $RETRIES); do
    if python3 - <<'PY' >/dev/null 2>&1
import xmlrpc.client
s = xmlrpc.client.ServerProxy('http://localhost:9000', allow_none=True)
s.system.listMethods()
PY
    then
        echo -e "${GREEN}✓${NC} Spix RPC is ready"
        break
    fi
    if [ $i -eq $RETRIES ]; then
        echo -e "${RED}✗${NC} Spix RPC did not become ready in time"
        echo "qnujitsi log: $TEST_ROOT/spix/qnujitsi_spix.log"
        exit 1
    fi
    sleep 0.2
done

echo ""
echo "--- Test Execution (Python) ---"

# Provide test parameters
# export JITSI_SERVER="$JITSI_SERVER"
export JITSI_SERVER="localhost"
export JITSI_ROOM="$JITSI_ROOM"
export SPIX_URL="http://localhost:9000"
export SPIX_ROOT="window"

python3 "$TEST_ROOT/spix/tests/run_all.py" || TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "========================================"
    echo -e "${GREEN}All tests PASSED!${NC}"
    echo "========================================"
else
    echo "========================================"
    echo -e "${RED}Some tests FAILED${NC}"
    echo "========================================"
    echo "Review output above for details"
    echo "qnujitsi log: $TEST_ROOT/spix/qnujitsi_spix.log"
fi

echo ""
echo "Logs available:"
echo "  Jitsi: cd $TEST_ROOT/docker && docker-compose logs"
echo "  gst-meet bot: docker logs gst-meet-bot1"
echo ""

exit $TEST_RESULT
