#include "participant_manager.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>

#include <gst/gst.h>
#include <gst/gstdebugutils.h>

ParticipantManager::ParticipantManager(GstElement* pipeline, GstElement* jitsibin)
  : pipeline_(pipeline), jitsibin_(jitsibin) {}

void ParticipantManager::connectSignals() {
  g_signal_connect(jitsibin_, "pad-added", GCallback(&ParticipantManager::onPadAdded), this);
  g_signal_connect(jitsibin_, "finished", GCallback(&ParticipantManager::onFinished), this);
}

bool ParticipantManager::initializeSlots(QQuickWindow* rootWindow) {
  // Create per-slot receive chains for each predeclared QML item: videoItem0, videoItem1, ...
  for (int i = 0; ; ++i) {
    QString objName = QString("videoItem%1").arg(i);
    QQuickItem* item = rootWindow->findChild<QQuickItem*>(objName.toUtf8().constData());
    if (!item) break;

    GstElement* vconv = gst_element_factory_make("videoconvert", nullptr);
    GstElement* glup = gst_element_factory_make("glupload", nullptr);
    GstElement* glcc = gst_element_factory_make("glcolorconvert", nullptr);
    GstElement* vsink = gst_element_factory_make("qml6glsink", nullptr);
    if (!vconv || !glup || !glcc || !vsink) {
      g_printerr("Failed to create receive elements for slot %d\n", i);
      return false;
    }
    gst_bin_add_many(GST_BIN(pipeline_), vconv, glup, glcc, vsink, NULL);
    if (!gst_element_link_many(vconv, glup, glcc, vsink, NULL)) {
      g_printerr("Failed to link videoconvert -> glupload -> glcolorconvert -> qml6glsink for slot %d\n", i);
      return false;
    }
    g_object_set(G_OBJECT(vsink),
                 "widget", item,
                 "sync", FALSE,
                 "async", FALSE,
                 "qos", TRUE,
                 NULL);

    videoconverts_.push_back(vconv);
    gluploads_.push_back(glup);
    glcolorconverts_.push_back(glcc);
    sinks_.push_back(vsink);
    inUse_.push_back(false);
  }
  g_print("Prepared %zu video slots\n", sinks_.size());
  return !sinks_.empty();
}

void ParticipantManager::onPadAdded(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self) return;
  self->handlePadAdded(pad);
}

void ParticipantManager::onFinished(GstElement* /*jitsibin*/, gboolean success, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self) return;
  self->handleFinished(success);
}

int ParticipantManager::acquireFreeSlot() const {
  for (size_t i = 0; i < inUse_.size(); ++i) {
    if (!inUse_[i]) return static_cast<int>(i);
  }
  return -1;
}

void ParticipantManager::handleFinished(gboolean success) {
  g_print("finished success=%d\n", success);
  if (!pipeline_) return;
  GstBus* bus = gst_element_get_bus(pipeline_);
  if (bus) {
    gst_bus_post(bus, gst_message_new_eos(nullptr));
    gst_object_unref(bus);
  }
}

void ParticipantManager::handlePadAdded(GstPad* pad) {
  if (!pipeline_) return;

  // Treat as raw H.264; jitsibin exposes depayloaded H.264
  g_print("Incoming pad %s: assuming raw H.264\n", GST_PAD_NAME(pad));

  // Choose a free video slot
  int slot_index = acquireFreeSlot();
  if (slot_index < 0) {
    g_printerr("No available video slots; ignoring new incoming pad %s\n", GST_PAD_NAME(pad));
    return;
  }

  // Explicit H.264 receive chain: h264parse -> (q_pre ~200ms) -> vtdec/avdec_h264 -> queue(post-decoder) -> slot videoconvert
  GstElement* parse = gst_element_factory_make("h264parse", nullptr);
  GstElement* dec = gst_element_factory_make("vtdec", nullptr);
  if (!dec) dec = gst_element_factory_make("avdec_h264", nullptr);
  // Pre-decoder queue to absorb ~200ms jitter before VideoToolbox
  GstElement* q_pre = gst_element_factory_make("queue", nullptr);
  // Single post-decoder queue with modest bounds
  GstElement* q_down = gst_element_factory_make("queue", nullptr);
  if (!parse || !dec || !q_pre || !q_down) {
    g_printerr("Failed to create H264 receive chain elements\n");
    return;
  }
  // Force parser to not passthrough and normalize to avc/au
  g_object_set(G_OBJECT(parse),
              //  "disable-passthrough", TRUE,
              "config-interval", 0,
               NULL);
  // if (g_object_class_find_property(G_OBJECT_GET_CLASS(dec), "realtime")) {
  //   g_object_set(G_OBJECT(dec), "realtime", TRUE, NULL);
  // }
  // Configure pre-decoder buffering to smooth bursty RTP arrival
  g_object_set(G_OBJECT(q_pre),
               "leaky", 0,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 1500000000, /* ~200 ms */
               NULL);
  // Optional small post-decode queue (defaults used)
  // g_object_set(G_OBJECT(q_down),
  //              "leaky", 0 /* non-leaky */,
  //              "max-size-buffers", 0,
  //              "max-size-bytes", 0,
  //              "max-size-time", 200000000 /* 200ms */,
  //              "flush-on-eos", TRUE,
  //              NULL);

  gst_bin_add_many(GST_BIN(pipeline_), parse, q_pre, dec, q_down, NULL);
  gst_element_sync_state_with_parent(parse);
  gst_element_sync_state_with_parent(q_pre);
  gst_element_sync_state_with_parent(dec);
  gst_element_sync_state_with_parent(q_down);

  // Link jitsibin pad -> h264parse
  {
    GstPad* sinkpad = gst_element_get_static_pad(parse, "sink");
    if (!sinkpad) return;
    GstPadLinkReturn linkret = gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
    if (linkret != GST_PAD_LINK_OK) {
      g_printerr("Failed to link jitsibin pad -> h264parse\n");
      return;
    }
  }
  // Link parse -> q_pre with caps enforcing avc/au for decoder compatibility, then q_pre -> dec
  {
    GstCaps* dec_caps = gst_caps_new_simple("video/x-h264",
                                            "stream-format", G_TYPE_STRING, "avc",
                                            "alignment", G_TYPE_STRING, "au",
                                            NULL);
    if (!gst_element_link_filtered(parse, q_pre, dec_caps)) {
      g_printerr("Failed to link h264parse -> pre-decode queue with filtered caps (avc/au)\n");
      gst_caps_unref(dec_caps);
      return;
    }
    gst_caps_unref(dec_caps);
    if (!gst_element_link(q_pre, dec)) {
      g_printerr("Failed to link pre-decode queue -> decoder\n");
      return;
    }
  }
  // Link dec -> q_down
  if (!gst_element_link(dec, q_down)) {
    g_printerr("Failed to link decoder -> downstream queue\n");
    return;
  }
  // Link q_down -> slot videoconvert
  if (!gst_element_link(q_down, videoconverts_[slot_index])) {
    g_printerr("Failed to link downstream queue -> videoconvert (slot %d)\n", slot_index);
    return;
  }
  g_print("H264 receive chain linked OK (slot %d) via raw h264\n", slot_index);

  inUse_[slot_index] = true;

  char dot_name[128];
  g_snprintf(dot_name, sizeof(dot_name), "receive_%s", GST_PAD_NAME(pad));
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            dot_name);
}


