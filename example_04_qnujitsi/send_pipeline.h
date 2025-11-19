// SendPipeline: encapsulates the local AV send path into jitsibin
#pragma once

#include <gst/gst.h>

class SendPipeline {
public:
  SendPipeline(GstElement* pipeline, GstElement* jitsibin);
  ~SendPipeline();

  // Build, configure and link the send chain.
  // localSlotVideoconvert: if non-null, tee raw video to this element for local preview
  // cameraDeviceIndex: camera device index for avfvideosrc (e.g., "0", "1", etc.)
  //                    if empty or nullptr, uses videotestsrc
  // Returns false on failure.
  bool buildAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert = nullptr,
                    const char* cameraDeviceIndex = nullptr);

  // Sync all elements with parent pipeline state (for hot-swapping)
  bool syncStateWithParent();

private:
  bool createElements(bool withLocalPreview, bool useCamera);
  void configureElements(int videoWidth, int videoHeight, const char* cameraDeviceIndex);
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
  GstElement* audiotestsrc_ {nullptr};
  GstElement* opusenc_ {nullptr};

  bool useCamera_ {false};
};


