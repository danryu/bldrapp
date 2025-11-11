// SendPipeline: encapsulates the local AV send path into jitsibin
#pragma once

#include <gst/gst.h>

class SendPipeline {
public:
  SendPipeline(GstElement* pipeline, GstElement* jitsibin);

  // Build, configure and link the send chain.
  // Returns false on failure.
  bool buildAndLink(int videoWidth, int videoHeight);

private:
  bool createElements();
  void configureElements(int videoWidth, int videoHeight);
  bool addToBinAndLink(int videoWidth, int videoHeight);

private:
  GstElement* pipeline_ {nullptr};
  GstElement* jitsibin_ {nullptr};

  // Video send
  GstElement* videotestsrc_ {nullptr};
  GstElement* videoscale_ {nullptr};
  GstElement* vtenc_ {nullptr};
  GstElement* h264parse_ {nullptr};
  GstElement* video_queue_ {nullptr};

  // Audio send
  GstElement* audiotestsrc_ {nullptr};
  GstElement* opusenc_ {nullptr};
};


