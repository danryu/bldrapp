// SendPipeline: encapsulates the local AV send path into jitsibin
#pragma once

#include <gst/gst.h>

class SendPipeline {
public:
  SendPipeline(GstElement* pipeline, GstElement* jitsibin);
  ~SendPipeline();

  // Build, configure and link the send chain.
  // localSlotVideoconvert: if non-null, tee raw video to this element for local preview
  // cameraDevice: GstDevice for camera, if nullptr uses videotestsrc
  // audioDeviceIndex: audio device index for osxaudiosrc (e.g., "0", "1", etc.)
  //                   if empty or nullptr, uses default audio device
  // Returns false on failure.
  bool buildAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert = nullptr,
                    GstDevice* cameraDevice = nullptr, const char* audioDeviceIndex = nullptr);

  // Sync all elements with parent pipeline state (for hot-swapping)
  bool syncStateWithParent();

  // Mute controls
  bool setVideoMuted(bool muted, GstDevice* newDevice = nullptr);
  bool setAudioMuted(bool muted);
  bool isVideoMuted() const { return videoMuted_; }
  bool isAudioMuted() const { return audioMuted_; }

  // Hot-swap camera source while pipeline is running
  bool setCamera(GstDevice* newDevice);

private:
  bool createElements(bool withLocalPreview, GstDevice* cameraDevice);
  void configureElements(int videoWidth, int videoHeight, const char* audioDeviceIndex);
  bool addToBinAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert);

private:
  GstElement* pipeline_ {nullptr};
  GstElement* jitsibin_ {nullptr};

  // Video send
  GstElement* videosrc_ {nullptr};      // Either avfvideosrc or videotestsrc
  GstElement* videoscale_ {nullptr};
  GstElement* videoconvert_ {nullptr};  // For camera source color format conversion
  GstElement* videorate_ {nullptr};     // For camera framerate control
  GstElement* tee_ {nullptr};           // Tee for splitting raw video
  GstElement* local_queue_ {nullptr};   // Queue for local preview branch
  GstElement* vtenc_ {nullptr};
  GstElement* h264parse_ {nullptr};
  GstElement* video_queue_ {nullptr};

  // Audio send
  GstElement* audiosrc_ {nullptr};       // osxaudiosrc for microphone capture
  GstElement* audioconvert_ {nullptr};   // Format conversion for encoder
  GstElement* volume_ {nullptr};         // Volume control for audio muting
  GstElement* opusenc_ {nullptr};

  bool useCamera_ {false};
  bool videoMuted_ {false};
  bool audioMuted_ {false};

  // Platform-specific element tracking
  const char* encoderName_ {nullptr};
  const char* audioSourceName_ {nullptr};
  bool isHardwareEncoder_ {false};
};


