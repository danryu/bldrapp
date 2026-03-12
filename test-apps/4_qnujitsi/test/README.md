# qnujitsi Automated Test Suite

Comprehensive automated UI testing infrastructure for the qnujitsi Qt/QML GStreamer Jitsi Meet client.

## Overview

This test suite provides end-to-end automated testing for qnujitsi using:

- **Docker Compose**: Self-hosted Jitsi Meet server for controlled test environment
- **gst-meet**: GStreamer-based participant simulator for multi-party testing
- **Spix**: Qt/QML GUI automation framework for UI interaction (RPC mode)

## Features

- Automated conference connection/disconnection testing
- Bidirectional audio/video verification
- Mute control testing (audio and video)
- Multi-participant simulation
- Extensible architecture for future GStreamer metrics and A/V analysis

## Quick Start

### Prerequisites

1. **Docker** (20.10+) and **Docker Compose** (1.29+)
   ```bash
   docker --version
   docker-compose --version
   ```

2. **CMake** (3.16+)
   ```bash
   cmake --version
   ```

3. **Qt6** (same installation as main qnujitsi app)
   - Default path: `/Users/dan/qt6-static-build`
   - Make sure it is on your `CMAKE_PREFIX_PATH` when building qnujitsi

4. **qnujitsi built**
   ```bash
   cd ../build
   cmake .. -DCMAKE_PREFIX_PATH="/Users/dan/qt6-static-build;../test/deps/install;../test/deps/install/lib" -DQNUJITSI_ENABLE_SPIX=ON
   make
   ```

### One-Time Setup

Run the setup script to prepare the test environment:

```bash
cd test/scripts
./setup_test_env.sh
```

This will:
- Build the gst-meet Docker image
- Pull Jitsi Meet Docker images
- Build and install AnyRPC + Spix (local deps)
- Prepare Python Spix RPC test scripts (no C++ test build)

### Running Tests

Execute the full test suite:

```bash
cd test/scripts
./run_tests.sh
```

The script will:
1. Start Jitsi Meet server
2. Launch gst-meet participant simulator(s)
3. Launch qnujitsi (with embedded Spix RPC server)
4. Run Python Spix RPC tests (XML-RPC)
4. Clean up infrastructure
5. Report results

### Expected Output

```
========================================
qnujitsi Automated Test Suite
========================================

Step 1: Starting Jitsi Meet server...
Step 2: Waiting for Jitsi Meet to be ready...
✓ Jitsi Meet is ready!

Step 3: Starting gst-meet participant simulators...
✓ TestBot1 started

Step 4: Running GUI tests...
--- Test Execution ---

--- Test 1: Conference Connection ---
Test 1 PASSED: Successfully connected to conference

--- Test 2: Audio/Video Both Ways ---
Test 2 PASSED: Audio/video bidirectional with mute controls

--- Test 3: Disconnection ---
Test 3 PASSED: Clean disconnection

========================================
All tests PASSED!
========================================
```

## Directory Structure

```
test/
├── docker/                     # Jitsi Meet Docker setup
│   ├── docker-compose.yml      # Jitsi services configuration
│   ├── .env                    # Environment variables
│   └── README.md               # Docker setup documentation
├── gst-meet/                   # Participant simulator
│   ├── Dockerfile              # gst-meet container image
│   ├── run_participant.sh      # Participant launch script
│   └── config.json             # Configuration reference
├── spix/                       # GUI automation tests
│   ├── tests/                  # Python Spix XML-RPC tests
│   │   ├── run_all.py
│   │   └── test_basic_flow.py
│   └── qnujitsi_spix.log        # qnujitsi log captured by run_tests.sh (on failure)
├── scripts/                    # Orchestration scripts
│   ├── setup_test_env.sh       # One-time environment setup
│   ├── run_tests.sh            # Full test suite execution
│   └── cleanup.sh              # Infrastructure cleanup
└── README.md                   # This file
```

## Test Scenarios

### Test 1: Conference Connection
- Launches qnujitsi application
- Verifies initial UI state
- Enters connection parameters (host, room)
- Clicks Connect button
- Waits for connection establishment
- Verifies connected UI state
- Confirms local video slot (slot 0) is active

### Test 2: Audio and Video Both Ways
- Ensures conference is connected
- Waits for remote participant (gst-meet bot) to join
- Verifies participant count and video slot visibility
- Tests video mute/unmute functionality
- Tests audio mute/unmute functionality
- Validates UI state changes for mute controls

### Test 3: Disconnection
- Clicks Disconnect button
- Waits for disconnection
- Verifies UI returns to disconnected state
- Confirms all video slots are hidden

## Manual Testing

### Start Jitsi Meet Server Only

```bash
cd test/docker
docker-compose up -d
```

Access web interface: http://localhost:8000

### Start gst-meet Participant Only

```bash
docker run -d --name gst-meet-bot1 --network host \
    -e JITSI_SERVER="localhost:8000" \
    -e JITSI_ROOM="testroom" \
    -e PARTICIPANT_NAME="TestBot1" \
    qnujitsi-gst-meet:latest
```

View logs:
```bash
docker logs -f gst-meet-bot1
```

### Run qnujitsi Manually

```bash
cd ../build
./qnujitsi
```

Configure:
- Host: `localhost:8000`
- Room: `testroom`
- Click Connect

You should see TestBot1 appear in the video grid.

### Run Spix Tests Only

```bash
cd test/docker && docker-compose up -d
cd test/scripts && ./run_tests.sh
```

## Cleanup

### Quick Cleanup
```bash
cd test/scripts
./cleanup.sh
```

### Manual Cleanup
```bash
# Stop containers
docker stop gst-meet-bot1 && docker rm gst-meet-bot1

# Stop Jitsi
cd test/docker
docker-compose down -v
```

## Troubleshooting

### Jitsi Server Won't Start

**Problem**: Services fail to start or aren't accessible

**Solutions**:
1. Check port availability:
   ```bash
   lsof -i :8000
   lsof -i :30443
   ```
2. View service logs:
   ```bash
   cd test/docker
   docker-compose logs -f
   ```
3. Verify Docker resources (need ~2GB RAM)

### gst-meet Won't Connect

**Problem**: Participant simulator fails to join conference

**Solutions**:
1. Check gst-meet logs:
   ```bash
   docker logs gst-meet-bot1
   ```
2. Verify Jitsi is accessible:
   ```bash
   curl http://localhost:8000
   ```
3. Ensure room name matches between qnujitsi and gst-meet

### Spix Tests Fail

**Problem**: GUI tests fail or timeout

**Solutions**:
1. Verify qnujitsi binary path:
   ```bash
   ls -l ../build/qnujitsi
   ```
2. Check prerequisites (Jitsi running, gst-meet joined)
3. Run tests with verbose output:
   ```bash
   cd test/spix/build
   ./qnujitsi_test --verbose
   ```
4. Test manually first to ensure basic functionality

### On macOS: JVB Connection Issues

**Problem**: Media (audio/video) not flowing

**Solution**: Update `test/docker/.env`:
```bash
# Find your local IP
ifconfig | grep "inet "

# Set in .env (use your actual IP, not 127.0.0.1)
DOCKER_HOST_ADDRESS=192.168.1.100
```

### On Linux: Network Issues

**Problem**: Containers can't reach host

**Solution**: Use `--network host` or find host IP:
```bash
# Get host IP from container perspective
docker run --rm alpine ip route | grep default
```

## Configuration

### Changing Test Parameters

Edit [scripts/run_tests.sh](scripts/run_tests.sh:1):

```bash
JITSI_SERVER="localhost:8000"       # Jitsi server address
JITSI_ROOM="testroom"               # Room name
PARTICIPANT_COUNT=1                 # Number of gst-meet bots (1-2 recommended)
```

### Customizing gst-meet Participants

Edit video/audio sources when launching:

```bash
docker run -d --name custom-bot --network host \
    -e JITSI_SERVER="localhost:8000" \
    -e JITSI_ROOM="testroom" \
    -e PARTICIPANT_NAME="CustomBot" \
    -e VIDEO_PATTERN="ball" \         # smpte, snow, ball, etc.
    -e AUDIO_WAVE="sine" \            # sine, square, triangle, etc.
    -e AUDIO_FREQ="880" \             # Frequency in Hz
    qnujitsi-gst-meet:latest
```

See [gst-meet/config.json](gst-meet/config.json:1) for pattern/wave options.

### Adjusting Timeouts

Edit [spix/test_basic_flow.cpp](spix/test_basic_flow.cpp:1):

```cpp
// Connection timeout (default 10s)
helper_->waitForConnection(10000);

// Participant join timeout (default 15s)
helper_->waitForParticipantSlot(1, 15000);

// Disconnection timeout (default 5s)
helper_->waitForDisconnection(5000);
```

## Future Enhancements

### Phase 2: GStreamer Pipeline Metrics

Planned additions:
- Monitor GStreamer bus messages (errors, state changes, EOS)
- Query pad capabilities and buffer statistics
- Verify jitsibin properties (jitter buffer, codec negotiation)
- Track pipeline state transitions

Implementation approach:
- Add `GstBusSpy` helper class
- Use `gst_element_get_static_pad()` and `gst_pad_get_current_caps()`
- Monitor `GST_MESSAGE_ERROR`, `GST_MESSAGE_STATE_CHANGED`, etc.

### Phase 3: Video and Audio Content Analysis

Planned additions:
- Video frame capture and analysis
  - Verify videotestsrc patterns (SMPTE bars, ball, etc.)
  - Detect motion and color changes
  - Use OpenCV for image processing
- Audio analysis
  - FFT to verify sine wave frequency (440 Hz, 880 Hz)
  - Volume level detection
  - Silence detection for mute verification

Implementation approach:
- Add `appsink` tee to receive pipelines
- Integrate OpenCV for frame analysis
- Use FFTW or similar for audio spectrum analysis

## CI/CD Integration

### GitHub Actions Example

```yaml
name: qnujitsi Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup test environment
        run: cd test/scripts && ./setup_test_env.sh
      - name: Run tests
        run: cd test/scripts && ./run_tests.sh
      - name: Upload logs
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: test-logs
          path: test/docker/logs/
```

## Contributing

When adding new tests:

1. Follow existing test structure in [test_basic_flow.cpp](spix/test_basic_flow.cpp:1)
2. Use `QnujitsiTestHelper` utilities where possible
3. Add appropriate timeouts and error messages
4. Update this README with new test scenarios

## References

- [Spix Documentation](https://github.com/faaxm/spix)
- [gst-meet Repository](https://github.com/avstack/gst-meet)
- [Jitsi Docker Setup](https://github.com/jitsi/docker-jitsi-meet)
- [qnujitsi Main README](../README.md)

## Support

For issues:
1. Check [Troubleshooting](#troubleshooting) section
2. Review logs (Jitsi, gst-meet, test output)
3. Verify prerequisites are installed correctly
4. Test components individually before full suite

## License

Same as qnujitsi project.
