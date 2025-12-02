#ifndef PLATFORM_ELEMENTS_H
#define PLATFORM_ELEMENTS_H

#include <gst/gst.h>

// Platform abstraction for GStreamer elements
// Handles Windows/macOS/iOS differences for video/audio encode/decode and device sources

// Video Encoder Configuration
struct VideoEncoderConfig {
    GstElement* element;
    const char* encoder_name;
    bool is_hardware;
};

// Video Decoder Configuration
struct VideoDecoderConfig {
    GstElement* element;
    const char* decoder_name;
    bool is_hardware;
};

// Audio Source Configuration
struct AudioSourceConfig {
    GstElement* element;
    const char* source_name;
};

// Audio Sink Configuration
struct AudioSinkConfig {
    GstElement* element;
    const char* sink_name;
};

// Create platform-appropriate video encoder (H.264)
// Tries hardware encoders first, falls back to software
VideoEncoderConfig createPlatformVideoEncoder();

// Configure video encoder with appropriate settings for real-time use
// Parameters:
//   config: VideoEncoderConfig from createPlatformVideoEncoder()
//   bitrate: target bitrate in kbps (e.g., 2500 for 2.5 Mbps)
void configurePlatformVideoEncoder(const VideoEncoderConfig& config, int bitrate);

// Create platform-appropriate video decoder (H.264)
// Tries hardware decoders first, falls back to software
VideoDecoderConfig createPlatformVideoDecoder();

// Configure video decoder with appropriate settings for real-time use
void configurePlatformVideoDecoder(const VideoDecoderConfig& config);

// Create platform-appropriate audio source (microphone)
AudioSourceConfig createPlatformAudioSource();

// Configure audio source with device selection
// deviceId: platform-specific device identifier (may be nullptr for default)
void configurePlatformAudioSource(const AudioSourceConfig& config, const char* deviceId);

// Create platform-appropriate audio sink (speaker/output)
AudioSinkConfig createPlatformAudioSink();

// Configure audio sink for real-time playback
void configurePlatformAudioSink(const AudioSinkConfig& config);

// Get the platform-appropriate camera source element name
// Used by CameraManager for device enumeration
const char* getPlatformCameraElementName();

// Get the platform-appropriate audio source element name
// Used by AudioManager for device enumeration
const char* getPlatformAudioSourceElementName();

#endif // PLATFORM_ELEMENTS_H
