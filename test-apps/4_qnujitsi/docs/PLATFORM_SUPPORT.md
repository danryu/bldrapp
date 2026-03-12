# Platform Support for qnujitsi

This document describes the platform abstraction layer implementation that enables qnujitsi to run on Windows, macOS, and iOS.

## Overview

The project now uses a platform abstraction layer to automatically select the appropriate GStreamer elements based on the target platform. Hardware acceleration is preferred when available, with automatic fallback to software codecs.

## Architecture

### Platform Abstraction Layer

Two new files provide the abstraction:

- **[platform_elements.h](platform_elements.h)** - Defines platform-agnostic interfaces
- **[platform_elements.cpp](platform_elements.cpp)** - Implements platform-specific element selection and configuration

### Platform Detection

The implementation uses preprocessor macros to detect the platform:
- `_WIN32` / `_WIN64` → Windows
- `__APPLE__` + `TARGET_OS_IOS` → iOS
- `__APPLE__` (without iOS) → macOS
- `__linux__` → Linux (for future support)

## Platform-Specific Elements

### Video Encoding

**Windows** (priority order):
1. `nvh264enc` - NVIDIA NVENC (hardware)
2. `amfh264enc` - AMD AMF (hardware)
3. `msdkh264enc` - Intel Media SDK (hardware)
4. `x264enc` - software fallback

**macOS/iOS**:
1. `vtenc_h264` - VideoToolbox (hardware)
2. `x264enc` - software fallback

**Linux**:
1. `nvh264enc` - NVIDIA NVENC (hardware)
2. `vah264enc` - VA-API (hardware, Intel/AMD)
3. `x264enc` - software fallback

### Video Decoding

**Windows** (priority order):
1. `nvh264dec` - NVIDIA NVDEC (hardware)
2. `amfh264dec` - AMD AMF (hardware)
3. `msdkh264dec` - Intel Media SDK (hardware)
4. `avdec_h264` - software fallback

**macOS/iOS**:
1. `vtdec` - VideoToolbox (hardware)
2. `avdec_h264` - software fallback

**Linux**:
1. `nvh264dec` - NVIDIA NVDEC (hardware)
2. `vah264dec` - VA-API (hardware, Intel/AMD)
3. `avdec_h264` - software fallback

### Audio Source (Microphone)

- **Windows**: `wasapisrc` (WASAPI)
- **macOS/iOS**: `osxaudiosrc` (CoreAudio)
- **Linux**: `pulsesrc` (PulseAudio)

### Audio Sink (Speakers)

- **Windows**: `wasapisink` (WASAPI)
- **macOS/iOS**: `osxaudiosink` (CoreAudio)
- **Linux**: `pulsesink` (PulseAudio)

### Camera Source

- **Windows**: `ksvideosrc` (Kernel Streaming)
- **macOS/iOS**: `avfvideosrc` (AVFoundation)
- **Linux**: `v4l2src` (Video4Linux2)

## Encoder-Specific Configuration

Each encoder is configured with appropriate low-latency settings:

### NVIDIA NVENC (Windows/Linux)
```cpp
preset: low-latency-hq (1)
rc-mode: vbr (3) - better quality than CBR in real-time
zerolatency: TRUE - disables B-frames
gop-size: 30
aud: TRUE - Access Unit Delimiters for better seeking
```

### AMD AMF (Windows)
```cpp
usage: ultra-low-latency (1) - optimized for real-time conferencing
rate-control: vbr (3) - better quality in real-time
gop-size: 30
```

### Intel Media SDK (Windows)
```cpp
rate-control: vbr (2) - better quality in real-time
target-usage: best-speed (1) - lowest latency
low-latency: TRUE
gop-size: 30
```

### VideoToolbox (macOS/iOS)
```cpp
realtime: TRUE
allow-frame-reordering: FALSE (disables B-frames)
```

### VA-API (Linux)
```cpp
rate-control: vbr (3) - better quality in real-time
bitrate: configured
key-int-max: 30
b-frames: 0 - disabled for lower latency
aud: TRUE - Access Unit Delimiters
```

### x264 Software (All platforms)
```cpp
tune: zerolatency (0x00000004) - no B-frames, minimal latency
speed-preset: superfast (1)
key-int-max: 30
bitrate: configured
```

## Decoder Configuration

All decoders are optimized for corruption-free real-time decoding:

### VideoToolbox (macOS/iOS)
```cpp
automatic-request-sync-points: TRUE
automatic-request-sync-point-flags: GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT
```
- Automatically requests keyframes when corruption is detected
- Outputs corrupted frames flagged for error concealment

### FFmpeg Software Decoder (avdec_h264)
```cpp
max-errors: -1 - unlimited errors before giving up
skip-frame: 0 - never skip frames, decode all
```
- Enables error concealment for corrupted frames
- Continues decoding despite errors

### Hardware Decoders (NVIDIA, AMD, Intel, VA-API)
- Hardware decoders handle corruption resilience automatically
- No special configuration needed

## Modified Files

### New Files
1. **[platform_elements.h](platform_elements.h)** - Platform abstraction interface
2. **[platform_elements.cpp](platform_elements.cpp)** - Platform-specific implementations

### Updated Files
1. **[send_pipeline.cpp](send_pipeline.cpp)** - Uses platform abstraction for encoder and audio source
2. **[participant_manager.cpp](participant_manager.cpp)** - Uses platform abstraction for decoder and audio sink
3. **[camera_manager.cpp](camera_manager.cpp)** - Uses `getPlatformCameraElementName()`
4. **[audio_manager.cpp](audio_manager.cpp)** - Uses `getPlatformAudioSourceElementName()`
5. **[CMakeLists.txt](CMakeLists.txt)** - Added `platform_elements.cpp` to build

## Building for Different Platforms

### macOS (Current)
```bash
rm -fr build/ && cmake -B build -S . && cmake --build build -j
./build/qnujitsi
```

### Windows
Requirements:
- GStreamer with Windows plugins (gst-plugins-bad for hardware encoders)
- Qt6 for Windows
- Visual Studio or MinGW

```bash
# Update CMakeLists.txt Qt path for Windows
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### iOS
Requirements:
- Xcode
- GStreamer iOS frameworks
- Qt6 for iOS

```bash
# Configure for iOS
cmake -B build -S . \
  -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64

cmake --build build --config Release
```

## Testing

### Testing Platform Detection
The application will print which encoder/decoder it's using at startup:
```
Using NVIDIA NVENC H.264 encoder
Configured nvh264enc: bitrate=2500 kbps, preset=low-latency-hq
Using NVIDIA NVDEC H.264 decoder
Using WASAPI audio source
Using WASAPI audio sink
```

### Testing Hardware Acceleration

**On Windows:**
1. Check Task Manager → Performance → GPU to see video encode/decode activity
2. For NVIDIA: Use `nvidia-smi` to monitor NVENC/NVDEC usage
3. For AMD: Use AMD Software performance monitoring
4. For Intel: Use Intel GPU Top tool

**On macOS:**
1. Check Activity Monitor → GPU History for VideoToolbox activity
2. The hardware encoder should show significantly lower CPU usage than software

### Testing Fallback
To test software fallback, temporarily rename the hardware encoder plugin:
```bash
# macOS: Disable VideoToolbox
GST_PLUGIN_PATH=/nonexistent ./build/qnujitsi

# Should see: "Using x264 software H.264 encoder"
```

## Known Platform Differences

### Windows
- WASAPI device enumeration uses GUID strings (not integer IDs)
- Hardware encoder availability depends on GPU vendor
- May need to install GPU vendor's SDK (NVIDIA Video Codec SDK, AMD AMF SDK, Intel Media SDK)

### macOS
- VideoToolbox always available (built into OS)
- CoreAudio device IDs are integers
- AVFoundation camera enumeration is stable

### iOS
- Same as macOS for codecs (VideoToolbox)
- Camera permissions required in Info.plist
- Microphone permissions required in Info.plist
- May need background audio capability for conference calls

## Troubleshooting

### "Failed to create video encoder"
- **Windows**: Install GPU drivers and ensure GStreamer bad plugins are installed
- **macOS**: Verify GStreamer installation includes VideoToolbox plugin
- **All**: Check that `x264enc` is available as fallback: `gst-inspect-1.0 x264enc`

### "No hardware encoder available, falling back to software"
- Normal on systems without GPU hardware acceleration
- Software encoding will use more CPU but still works

### Audio/Video Device Not Found
- **Windows**: Check Windows audio/video settings and permissions
- **macOS**: Check System Preferences → Security & Privacy → Camera/Microphone
- **iOS**: Ensure Info.plist has required permission keys

## Future Enhancements

1. **Dynamic bitrate adjustment** - Adjust encoder bitrate based on network conditions
2. **VP8/VP9 support** - Add codec selection for non-H.264 codecs
3. **Android support** - Add Android MediaCodec elements
4. **Hardware-accelerated color conversion** - Use GPU for format conversion on Windows

## References

- [GStreamer Plugin Documentation](https://gstreamer.freedesktop.org/documentation/plugins_doc.html)
- [NVIDIA NVENC Plugin](https://gstreamer.freedesktop.org/documentation/nvcodec/index.html)
- [AMD AMF Plugin](https://github.com/GPUOpen-LibrariesAndSDKs/AMF)
- [Intel Media SDK Plugin](https://gstreamer.freedesktop.org/documentation/msdk/index.html)
- [VideoToolbox Plugin](https://gstreamer.freedesktop.org/documentation/applemedia/index.html)
