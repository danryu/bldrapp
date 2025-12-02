#include "platform_elements.h"
#include <gst/video/gstvideodecoder.h>
#include <string.h>

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
        #define PLATFORM_IOS
    #else
        #define PLATFORM_MACOS
    #endif
#elif defined(__linux__)
    #define PLATFORM_LINUX
#endif

// =============================================================================
// Video Encoder
// =============================================================================

VideoEncoderConfig createPlatformVideoEncoder() {
    VideoEncoderConfig config = {nullptr, nullptr, false};

#ifdef PLATFORM_WINDOWS
    // Windows: Try NVIDIA -> AMD -> Intel -> software fallback

    // Try NVIDIA NVENC (best performance on NVIDIA GPUs)
    config.element = gst_element_factory_make("nvh264enc", nullptr);
    if (config.element) {
        config.encoder_name = "nvh264enc";
        config.is_hardware = true;
        g_print("Using NVIDIA NVENC H.264 encoder\n");
        return config;
    }

    // Try AMD AMF (best performance on AMD GPUs)
    config.element = gst_element_factory_make("amfh264enc", nullptr);
    if (config.element) {
        config.encoder_name = "amfh264enc";
        config.is_hardware = true;
        g_print("Using AMD AMF H.264 encoder\n");
        return config;
    }

    // Try Intel Media SDK (best performance on Intel GPUs)
    config.element = gst_element_factory_make("msdkh264enc", nullptr);
    if (config.element) {
        config.encoder_name = "msdkh264enc";
        config.is_hardware = true;
        g_print("Using Intel Media SDK H.264 encoder\n");
        return config;
    }

    g_print("No hardware encoder available on Windows, falling back to software\n");

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    // macOS/iOS: Use VideoToolbox
    config.element = gst_element_factory_make("vtenc_h264", nullptr);
    if (config.element) {
        config.encoder_name = "vtenc_h264";
        config.is_hardware = true;
        g_print("Using VideoToolbox H.264 encoder\n");
        return config;
    }

    g_print("VideoToolbox encoder not available, falling back to software\n");

#elif defined(PLATFORM_LINUX)
    // Linux: Try NVIDIA -> VA-API -> software fallback

    // Try NVIDIA NVENC (works on Linux with NVIDIA GPUs)
    config.element = gst_element_factory_make("nvh264enc", nullptr);
    if (config.element) {
        config.encoder_name = "nvh264enc";
        config.is_hardware = true;
        g_print("Using NVIDIA NVENC H.264 encoder\n");
        return config;
    }

    // Try VA-API (Intel/AMD GPUs on Linux)
    config.element = gst_element_factory_make("vah264enc", nullptr);
    if (config.element) {
        config.encoder_name = "vah264enc";
        config.is_hardware = true;
        g_print("Using VA-API H.264 encoder\n");
        return config;
    }

    g_print("No hardware encoder available on Linux, falling back to software\n");
#endif

    // Software fallback (all platforms)
    config.element = gst_element_factory_make("x264enc", nullptr);
    if (config.element) {
        config.encoder_name = "x264enc";
        config.is_hardware = false;
        g_print("Using x264 software H.264 encoder\n");
        return config;
    }

    g_printerr("Failed to create any H.264 encoder\n");
    return config;
}

void configurePlatformVideoEncoder(const VideoEncoderConfig& config, int bitrate) {
    if (!config.element) return;

    if (strcmp(config.encoder_name, "nvh264enc") == 0) {
        // NVIDIA NVENC configuration for ultra-low-latency real-time encoding
        g_object_set(G_OBJECT(config.element),
                     "preset", 1,           // low-latency-hq (1) for quality with low latency
                     "rc-mode", 3,          // vbr (3) for better quality than cbr in real-time
                     "bitrate", bitrate,    // bitrate in kbps
                     "zerolatency", TRUE,   // disable frame reordering (no B-frames)
                     "gop-size", 30,        // keyframe every 30 frames (1 sec at 30fps)
                     "aud", TRUE,           // insert Access Unit Delimiters for better seeking
                     NULL);
        g_print("Configured nvh264enc: bitrate=%d kbps, preset=low-latency-hq, rc-mode=vbr, zerolatency=TRUE\n", bitrate);
    }
    else if (strcmp(config.encoder_name, "amfh264enc") == 0) {
        // AMD AMF configuration for ultra-low-latency real-time encoding
        g_object_set(G_OBJECT(config.element),
                     "usage", 1,            // ultra-low-latency (1) for real-time conferencing
                     "rate-control", 3,     // vbr (3) for better quality in real-time
                     "bitrate", bitrate,    // bitrate in kbps
                     "gop-size", 30,        // keyframe every 30 frames
                     NULL);
        g_print("Configured amfh264enc: bitrate=%d kbps, usage=ultra-low-latency, rc-mode=vbr\n", bitrate);
    }
    else if (strcmp(config.encoder_name, "msdkh264enc") == 0) {
        // Intel Media SDK configuration for low-latency real-time encoding
        g_object_set(G_OBJECT(config.element),
                     "rate-control", 2,     // vbr (2) for better quality in real-time
                     "bitrate", bitrate,    // bitrate in kbps
                     "gop-size", 30,        // keyframe every 30 frames
                     "target-usage", 1,     // best speed (1) for lowest latency
                     "low-latency", TRUE,   // enable low-latency mode
                     NULL);
        g_print("Configured msdkh264enc: bitrate=%d kbps, target-usage=best-speed, low-latency=TRUE\n", bitrate);
    }
    else if (strcmp(config.encoder_name, "vtenc_h264") == 0) {
        // VideoToolbox (macOS/iOS) configuration for low-latency real-time encoding
        g_object_set(G_OBJECT(config.element),
                     "realtime", TRUE,              // real-time encoding mode
                     "allow-frame-reordering", FALSE, // disable B-frames for lower latency
                     "bitrate", bitrate,            // bitrate in kbps
                     NULL);
        g_print("Configured vtenc_h264: bitrate=%d kbps, realtime=TRUE\n", bitrate);
    }
    else if (strcmp(config.encoder_name, "vah264enc") == 0) {
        // VA-API (Linux) configuration for low-latency real-time encoding
        g_object_set(G_OBJECT(config.element),
                     "rate-control", 3,     // vbr (3) for better quality in real-time
                     "bitrate", bitrate,    // bitrate in kbps
                     "key-int-max", 30,     // keyframe every 30 frames
                     "b-frames", 0,         // disable B-frames for lower latency
                     "aud", TRUE,           // insert Access Unit Delimiters
                     NULL);
        g_print("Configured vah264enc: bitrate=%d kbps, rate-control=vbr, b-frames=0\n", bitrate);
    }
    else if (strcmp(config.encoder_name, "x264enc") == 0) {
        // x264 software encoder configuration for low-latency real-time encoding
        g_object_set(G_OBJECT(config.element),
                     "tune", 0x00000004,    // zerolatency
                     "speed-preset", 1,     // superfast (balance quality/performance)
                     "bitrate", bitrate,    // bitrate in kbps
                     "key-int-max", 30,     // keyframe every 30 frames
                     NULL);
        g_print("Configured x264enc: bitrate=%d kbps, tune=zerolatency, preset=superfast\n", bitrate);
    }
}

// =============================================================================
// Video Decoder
// =============================================================================

VideoDecoderConfig createPlatformVideoDecoder() {
    VideoDecoderConfig config = {nullptr, nullptr, false};

#ifdef PLATFORM_WINDOWS
    // Windows: Try NVIDIA -> AMD -> Intel -> software fallback

    // Try NVIDIA NVDEC
    config.element = gst_element_factory_make("nvh264dec", nullptr);
    if (config.element) {
        config.decoder_name = "nvh264dec";
        config.is_hardware = true;
        g_print("Using NVIDIA NVDEC H.264 decoder\n");
        return config;
    }

    // Try AMD AMF decoder
    config.element = gst_element_factory_make("amfh264dec", nullptr);
    if (config.element) {
        config.decoder_name = "amfh264dec";
        config.is_hardware = true;
        g_print("Using AMD AMF H.264 decoder\n");
        return config;
    }

    // Try Intel Media SDK decoder
    config.element = gst_element_factory_make("msdkh264dec", nullptr);
    if (config.element) {
        config.decoder_name = "msdkh264dec";
        config.is_hardware = true;
        g_print("Using Intel Media SDK H.264 decoder\n");
        return config;
    }

    g_print("No hardware decoder available on Windows, falling back to software\n");

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    // macOS/iOS: Use VideoToolbox
    config.element = gst_element_factory_make("vtdec", nullptr);
    if (config.element) {
        config.decoder_name = "vtdec";
        config.is_hardware = true;
        g_print("Using VideoToolbox H.264 decoder\n");
        return config;
    }

    g_print("VideoToolbox decoder not available, falling back to software\n");

#elif defined(PLATFORM_LINUX)
    // Linux: Try NVIDIA -> VA-API -> software fallback

    // Try NVIDIA NVDEC (works on Linux with NVIDIA GPUs)
    config.element = gst_element_factory_make("nvh264dec", nullptr);
    if (config.element) {
        config.decoder_name = "nvh264dec";
        config.is_hardware = true;
        g_print("Using NVIDIA NVDEC H.264 decoder\n");
        return config;
    }

    // Try VA-API (Intel/AMD GPUs on Linux)
    config.element = gst_element_factory_make("vah264dec", nullptr);
    if (config.element) {
        config.decoder_name = "vah264dec";
        config.is_hardware = true;
        g_print("Using VA-API H.264 decoder\n");
        return config;
    }

    g_print("No hardware decoder available on Linux, falling back to software\n");
#endif

    // Software fallback (all platforms)
    config.element = gst_element_factory_make("avdec_h264", nullptr);
    if (config.element) {
        config.decoder_name = "avdec_h264";
        config.is_hardware = false;
        g_print("Using avdec_h264 software H.264 decoder\n");
        return config;
    }

    g_printerr("Failed to create any H.264 decoder\n");
    return config;
}

void configurePlatformVideoDecoder(const VideoDecoderConfig& config) {
    if (!config.element) return;

    if (strcmp(config.decoder_name, "nvh264dec") == 0) {
        // NVIDIA NVDEC decoder: Configure for error resilience
        // Note: nvh264dec automatically handles corrupted frames well
        g_print("Configured nvh264dec for real-time decoding\n");
    }
    else if (strcmp(config.decoder_name, "amfh264dec") == 0) {
        // AMD AMF decoder: Configure for error resilience
        g_print("Configured amfh264dec for real-time decoding\n");
    }
    else if (strcmp(config.decoder_name, "msdkh264dec") == 0) {
        // Intel Media SDK decoder: Configure for error resilience
        g_print("Configured msdkh264dec for real-time decoding\n");
    }
    else if (strcmp(config.decoder_name, "vtdec") == 0) {
        // VideoToolbox decoder: Enable automatic sync point recovery for corruption handling
        g_object_set(G_OBJECT(config.element),
                     "automatic-request-sync-points", TRUE,
                     "automatic-request-sync-point-flags", GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT,
                     NULL);
        g_print("Configured vtdec with automatic sync point recovery (corruption-free decoding)\n");
    }
    else if (strcmp(config.decoder_name, "vah264dec") == 0) {
        // VA-API decoder: Configure for error resilience
        g_print("Configured vah264dec for real-time decoding\n");
    }
    else if (strcmp(config.decoder_name, "avdec_h264") == 0) {
        // FFmpeg software decoder: Enable error concealment for corruption handling
        g_object_set(G_OBJECT(config.element),
                     "max-errors", -1,      // unlimited errors before giving up
                     "skip-frame", 0,       // never skip frames (decode all)
                     NULL);
        g_print("Configured avdec_h264 with error concealment for corruption-free decoding\n");
    }
}

// =============================================================================
// Audio Source (Microphone)
// =============================================================================

AudioSourceConfig createPlatformAudioSource() {
    AudioSourceConfig config = {nullptr, nullptr};

#ifdef PLATFORM_WINDOWS
    // Windows: Use WASAPI
    config.element = gst_element_factory_make("wasapisrc", nullptr);
    if (config.element) {
        config.source_name = "wasapisrc";
        g_print("Using WASAPI audio source\n");
        return config;
    }

    g_printerr("Failed to create wasapisrc on Windows\n");

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    // macOS/iOS: Use CoreAudio
    config.element = gst_element_factory_make("osxaudiosrc", nullptr);
    if (config.element) {
        config.source_name = "osxaudiosrc";
        g_print("Using CoreAudio audio source\n");
        return config;
    }

    g_printerr("Failed to create osxaudiosrc on macOS/iOS\n");

#elif defined(PLATFORM_LINUX)
    // Linux: Use PulseAudio
    config.element = gst_element_factory_make("pulsesrc", nullptr);
    if (config.element) {
        config.source_name = "pulsesrc";
        g_print("Using PulseAudio audio source\n");
        return config;
    }

    g_printerr("Failed to create pulsesrc on Linux\n");
#endif

    return config;
}

void configurePlatformAudioSource(const AudioSourceConfig& config, const char* deviceId) {
    if (!config.element) return;

    if (strcmp(config.source_name, "wasapisrc") == 0) {
        // WASAPI configuration
        // Device property expects a device GUID string on Windows
        if (deviceId && strlen(deviceId) > 0) {
            g_object_set(G_OBJECT(config.element),
                         "device", deviceId,
                         NULL);
            g_print("Configured wasapisrc with device: %s\n", deviceId);
        } else {
            g_print("Using default WASAPI audio device\n");
        }
    }
    else if (strcmp(config.source_name, "osxaudiosrc") == 0) {
        // CoreAudio configuration
        // Device property expects an integer AudioDeviceID
        if (deviceId && strlen(deviceId) > 0) {
            int audioDevice = atoi(deviceId);
            g_object_set(G_OBJECT(config.element),
                         "device", audioDevice,
                         NULL);
            g_print("Configured osxaudiosrc with device: %d\n", audioDevice);
        } else {
            g_print("Using default CoreAudio device\n");
        }
    }
    else if (strcmp(config.source_name, "pulsesrc") == 0) {
        // PulseAudio configuration
        if (deviceId && strlen(deviceId) > 0) {
            g_object_set(G_OBJECT(config.element),
                         "device", deviceId,
                         NULL);
            g_print("Configured pulsesrc with device: %s\n", deviceId);
        } else {
            g_print("Using default PulseAudio device\n");
        }
    }
}

// =============================================================================
// Audio Sink (Speaker/Output)
// =============================================================================

AudioSinkConfig createPlatformAudioSink() {
    AudioSinkConfig config = {nullptr, nullptr};

#ifdef PLATFORM_WINDOWS
    // Windows: Use WASAPI
    config.element = gst_element_factory_make("wasapisink", nullptr);
    if (config.element) {
        config.sink_name = "wasapisink";
        g_print("Using WASAPI audio sink\n");
        return config;
    }

    g_printerr("Failed to create wasapisink on Windows\n");

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    // macOS/iOS: Use CoreAudio
    config.element = gst_element_factory_make("osxaudiosink", nullptr);
    if (config.element) {
        config.sink_name = "osxaudiosink";
        g_print("Using CoreAudio audio sink\n");
        return config;
    }

    g_printerr("Failed to create osxaudiosink on macOS/iOS\n");

#elif defined(PLATFORM_LINUX)
    // Linux: Use PulseAudio
    config.element = gst_element_factory_make("pulsesink", nullptr);
    if (config.element) {
        config.sink_name = "pulsesink";
        g_print("Using PulseAudio audio sink\n");
        return config;
    }

    g_printerr("Failed to create pulsesink on Linux\n");
#endif

    return config;
}

void configurePlatformAudioSink(const AudioSinkConfig& config) {
    if (!config.element) return;

    // Configure for real-time playback without clock sync
    // CRITICAL: sync=FALSE prevents audio sink from affecting pipeline clock,
    // which would otherwise cause video frame rate to drop significantly.
    // In real-time conferencing, jitsibin's jitter buffer handles timing.
    g_object_set(G_OBJECT(config.element),
                 "sync", FALSE,   // Play audio immediately without clock sync
                 "async", FALSE,  // Don't affect pipeline state transitions
                 NULL);

    g_print("Configured %s for real-time playback (sync=FALSE, async=FALSE)\n", config.sink_name);
}

// =============================================================================
// Device Enumeration Helpers
// =============================================================================

const char* getPlatformCameraElementName() {
#ifdef PLATFORM_WINDOWS
    return "ksvideosrc";
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    return "avfvideosrc";
#elif defined(PLATFORM_LINUX)
    return "v4l2src";
#else
    return nullptr;  // No camera available on unknown platforms
#endif
}

const char* getPlatformAudioSourceElementName() {
#ifdef PLATFORM_WINDOWS
    return "wasapisrc";
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    return "osxaudiosrc";
#elif defined(PLATFORM_LINUX)
    return "pulsesrc";
#else
    return nullptr;  // No audio source available on unknown platforms
#endif
}
