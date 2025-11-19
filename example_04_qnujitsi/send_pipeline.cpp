#include "send_pipeline.h"

#include <gst/gst.h>

SendPipeline::SendPipeline(GstElement* pipeline, GstElement* jitsibin)
  : pipeline_(pipeline), jitsibin_(jitsibin) {}

bool SendPipeline::buildAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert) {
  if (!createElements(localSlotVideoconvert != nullptr)) return false;
  configureElements(videoWidth, videoHeight);
  return addToBinAndLink(videoWidth, videoHeight, localSlotVideoconvert);
}

bool SendPipeline::createElements(bool withLocalPreview) {
  videotestsrc_ = gst_element_factory_make("videotestsrc", nullptr);
  videoscale_   = gst_element_factory_make("videoscale", nullptr);
  
  // Create tee and local queue only if we're doing local preview
  if (withLocalPreview) {
    tee_ = gst_element_factory_make("tee", nullptr);
    local_queue_ = gst_element_factory_make("queue", nullptr);
  }
  
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
  
  // Check tee and local_queue if we're doing local preview
  if (withLocalPreview && (!tee_ || !local_queue_)) {
    g_printerr("Failed to create tee or local_queue for local preview.\n");
    return false;
  }
  
  return true;
}

void SendPipeline::configureElements(int /*videoWidth*/, int /*videoHeight*/) {
  // Configure encoder queue for low-latency (leaky downstream to prevent buffering)
  g_object_set(G_OBJECT(video_queue_),
               "max-size-buffers", 0,
               "max-size-time", 0,
               "max-size-bytes", 0,
               "leaky", 2, // GST_QUEUE_LEAK_DOWNSTREAM
               NULL);

  // Configure local preview queue if present
  if (local_queue_) {
    g_object_set(G_OBJECT(local_queue_),
                 "max-size-buffers", 5,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 "leaky", 2, // GST_QUEUE_LEAK_DOWNSTREAM - drop old frames
                 NULL);
  }

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

bool SendPipeline::addToBinAndLink(int videoWidth, int videoHeight, GstElement* localSlotVideoconvert) {
  // Add to bin
  if (localSlotVideoconvert) {
    // With local preview: add tee and local_queue
    gst_bin_add_many(GST_BIN(pipeline_),
                     // video path
                     videotestsrc_, videoscale_, tee_, local_queue_, vtenc_, h264parse_, video_queue_,
                     // audio path
                     audiotestsrc_, opusenc_,
                     NULL);
  } else {
    // Without local preview
    gst_bin_add_many(GST_BIN(pipeline_),
                     // video path
                     videotestsrc_, videoscale_, vtenc_, h264parse_, video_queue_,
                     // audio path
                     audiotestsrc_, opusenc_,
                     NULL);
  }

  g_print("=== Video send pipeline ===\n");
  if (localSlotVideoconvert) {
    g_print("videotestsrc -> videoscale -> tee\n");
    g_print("  tee branch 1: queue(leaky) -> videoconvert (slot 0 for local preview)\n");
    g_print("  tee branch 2: vtenc_h264 -> h264parse -> queue(leaky) -> jitsibin:video_sink\n");
  } else {
    g_print("videotestsrc -> videoscale -> vtenc_h264 -> h264parse -> queue(leaky) -> jitsibin:video_sink\n");
  }
  g_print("Encoder configured: realtime=TRUE, allow-frame-reordering=FALSE\n");

  // Link video source -> scale
  if (!gst_element_link(videotestsrc_, videoscale_)) {
    g_printerr("Failed to link videotestsrc -> videoscale\n");
    return false;
  }

  // Create caps for raw video (I420 format)
  GstCaps* raw_caps = gst_caps_new_simple("video/x-raw",
                                          "format", G_TYPE_STRING, "I420",
                                          "width", G_TYPE_INT, videoWidth,
                                          "height", G_TYPE_INT, videoHeight,
                                          "framerate", GST_TYPE_FRACTION, 30, 1,
                                          NULL);

  if (localSlotVideoconvert) {
    // Link videoscale -> tee with explicit caps
    if (!gst_element_link_filtered(videoscale_, tee_, raw_caps)) {
      g_printerr("Failed to link videoscale -> tee with filtered caps (I420/%dx%d@30)\n",
                 videoWidth, videoHeight);
      gst_caps_unref(raw_caps);
      return false;
    }
    
    // Branch 1: tee -> local_queue -> localSlotVideoconvert (for local preview)
    GstPad* tee_src_pad_local = gst_element_request_pad_simple(tee_, "src_%u");
    GstPad* queue_sink_pad = gst_element_get_static_pad(local_queue_, "sink");
    if (!tee_src_pad_local || !queue_sink_pad) {
      g_printerr("Failed to get pads for tee -> local_queue link\n");
      gst_caps_unref(raw_caps);
      if (tee_src_pad_local) gst_object_unref(tee_src_pad_local);
      if (queue_sink_pad) gst_object_unref(queue_sink_pad);
      return false;
    }
    if (gst_pad_link(tee_src_pad_local, queue_sink_pad) != GST_PAD_LINK_OK) {
      g_printerr("Failed to link tee -> local_queue\n");
      gst_caps_unref(raw_caps);
      gst_object_unref(tee_src_pad_local);
      gst_object_unref(queue_sink_pad);
      return false;
    }
    gst_object_unref(tee_src_pad_local);
    gst_object_unref(queue_sink_pad);
    
    // Link local_queue -> localSlotVideoconvert
    if (!gst_element_link(local_queue_, localSlotVideoconvert)) {
      g_printerr("Failed to link local_queue -> videoconvert (slot 0)\n");
      gst_caps_unref(raw_caps);
      return false;
    }
    
    // Branch 2: tee -> vtenc (encoder branch)
    GstPad* tee_src_pad_enc = gst_element_request_pad_simple(tee_, "src_%u");
    GstPad* enc_sink_pad = gst_element_get_static_pad(vtenc_, "sink");
    if (!tee_src_pad_enc || !enc_sink_pad) {
      g_printerr("Failed to get pads for tee -> vtenc link\n");
      gst_caps_unref(raw_caps);
      if (tee_src_pad_enc) gst_object_unref(tee_src_pad_enc);
      if (enc_sink_pad) gst_object_unref(enc_sink_pad);
      return false;
    }
    if (gst_pad_link(tee_src_pad_enc, enc_sink_pad) != GST_PAD_LINK_OK) {
      g_printerr("Failed to link tee -> vtenc\n");
      gst_caps_unref(raw_caps);
      gst_object_unref(tee_src_pad_enc);
      gst_object_unref(enc_sink_pad);
      return false;
    }
    gst_object_unref(tee_src_pad_enc);
    gst_object_unref(enc_sink_pad);
  } else {
    // No local preview: Link videoscale -> vtenc with explicit caps
    if (!gst_element_link_filtered(videoscale_, vtenc_, raw_caps)) {
      g_printerr("Failed to link videoscale -> vtenc_h264 with filtered caps (I420/%dx%d@30)\n",
                 videoWidth, videoHeight);
      gst_caps_unref(raw_caps);
      return false;
    }
  }
  
  gst_caps_unref(raw_caps);

  // Link vtenc -> h264parse -> queue (common path for both cases)
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


