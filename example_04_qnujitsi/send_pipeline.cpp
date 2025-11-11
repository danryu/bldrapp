#include "send_pipeline.h"

#include <gst/gst.h>

SendPipeline::SendPipeline(GstElement* pipeline, GstElement* jitsibin)
  : pipeline_(pipeline), jitsibin_(jitsibin) {}

bool SendPipeline::buildAndLink(int videoWidth, int videoHeight) {
  if (!createElements()) return false;
  configureElements(videoWidth, videoHeight);
  return addToBinAndLink(videoWidth, videoHeight);
}

bool SendPipeline::createElements() {
  videotestsrc_ = gst_element_factory_make("videotestsrc", nullptr);
  videoscale_   = gst_element_factory_make("videoscale", nullptr);
  vtenc_        = gst_element_factory_make("vtenc_h264", nullptr);
  h264parse_    = gst_element_factory_make("h264parse", nullptr);
  video_queue_  = gst_element_factory_make("queue", nullptr);
  audiotestsrc_ = gst_element_factory_make("audiotestsrc", nullptr);
  opusenc_      = gst_element_factory_make("opusenc", nullptr);

  if (!pipeline_ || !jitsibin_ ||
      !videotestsrc_ || !videoscale_ || !vtenc_ || !h264parse_ || !video_queue_ ||
      !audiotestsrc_ || !opusenc_) {
    g_printerr("Failed to create one or more send elements.\n");
    return false;
  }
  return true;
}

void SendPipeline::configureElements(int /*videoWidth*/, int /*videoHeight*/) {
  // Configure queue for low-latency (leaky downstream to prevent buffering)
  g_object_set(G_OBJECT(video_queue_),
               "max-size-buffers", 0,
               "max-size-time", 0,
               "max-size-bytes", 0,
               "leaky", 2, // GST_QUEUE_LEAK_DOWNSTREAM
               NULL);

  // Encoder low latency
  g_object_set(G_OBJECT(vtenc_),
               "realtime", TRUE,
               "allow-frame-reordering", FALSE,
               NULL);

  // h264parse: send SPS/PPS with every IDR
  g_object_set(G_OBJECT(h264parse_),
               "config-interval", 1,
               NULL);

  // Live sources
  g_object_set(G_OBJECT(videotestsrc_), "is-live", TRUE, NULL);
  g_object_set(G_OBJECT(audiotestsrc_), "is-live", TRUE, "wave", 8, NULL);
}

bool SendPipeline::addToBinAndLink(int videoWidth, int videoHeight) {
  // Add to bin
  gst_bin_add_many(GST_BIN(pipeline_),
                   // video path
                   videotestsrc_, videoscale_, vtenc_, h264parse_, video_queue_,
                   // audio path
                   audiotestsrc_, opusenc_,
                   NULL);

  g_print("=== Video send pipeline ===\n");
  g_print("videotestsrc -> videoscale -> vtenc_h264 -> h264parse -> queue(leaky) -> jitsibin:video_sink\n");
  g_print("Encoder configured: realtime=TRUE, allow-frame-reordering=FALSE\n");

  // Link video source -> scale
  if (!gst_element_link(videotestsrc_, videoscale_)) {
    g_printerr("Failed to link videotestsrc -> videoscale\n");
    return false;
  }

  // Link videoscale -> vtenc with explicit caps
  GstCaps* send_caps = gst_caps_new_simple("video/x-raw",
                                           "format", G_TYPE_STRING, "I420",
                                           "width", G_TYPE_INT, videoWidth,
                                           "height", G_TYPE_INT, videoHeight,
                                           "framerate", GST_TYPE_FRACTION, 30, 1,
                                           NULL);
  if (!gst_element_link_filtered(videoscale_, vtenc_, send_caps)) {
    g_printerr("Failed to link videoscale -> vtenc_h264 with filtered caps (I420/%dx%d@30)\n",
               videoWidth, videoHeight);
    gst_caps_unref(send_caps);
    return false;
  }
  gst_caps_unref(send_caps);

  // Link vtenc -> h264parse -> queue
  if (!gst_element_link(vtenc_, h264parse_)) {
    g_printerr("Failed to link vtenc_h264 -> h264parse\n");
    return false;
  }
  if (!gst_element_link(h264parse_, video_queue_)) {
    g_printerr("Failed to link h264parse -> queue\n");
    return false;
  }
  // Link queue to jitsibin pad
  if (!gst_element_link_pads(video_queue_, NULL, jitsibin_, "video_sink")) {
    g_printerr("Failed to link queue -> jitsibin video_sink\n");
    return false;
  }

  // Audio path
  if (!gst_element_link_many(audiotestsrc_, opusenc_, NULL)) {
    g_printerr("Failed to link audiotestsrc -> opusenc\n");
    return false;
  }
  if (!gst_element_link_pads(opusenc_, NULL, jitsibin_, "audio_sink")) {
    g_printerr("Failed to link opusenc -> jitsibin audio_sink\n");
    return false;
  }

  return true;
}


