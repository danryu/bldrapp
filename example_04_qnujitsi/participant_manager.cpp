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

  // Inspect pad caps to choose the appropriate receive path
  bool is_rtp_h264 = false;
  bool is_raw_h264 = false;
  {
    GstCaps* pcaps = gst_pad_get_current_caps(pad);
    if (!pcaps) pcaps = gst_pad_query_caps(pad, nullptr);
    if (pcaps) {
      char* caps_str = gst_caps_to_string(pcaps);
      g_print("Incoming pad %s caps: %s\n", GST_PAD_NAME(pad), caps_str ? caps_str : "(null)");
      g_free(caps_str);
      if (GstStructure* s = gst_caps_get_structure(pcaps, 0)) {
        const char* name = gst_structure_get_name(s);
        if (name) {
          if (g_strcmp0(name, "application/x-rtp") == 0) {
            const char* enc = gst_structure_get_string(s, "encoding-name");
            if (enc && g_ascii_strcasecmp(enc, "H264") == 0) is_rtp_h264 = true;
          } else if (g_strcmp0(name, "video/x-h264") == 0) {
            is_raw_h264 = true;
          }
        }
      }
      gst_caps_unref(pcaps);
    }
  }
  if (!is_rtp_h264 && !is_raw_h264) {
    // Default to RTP H264 if undetermined; Jitsi typically delivers RTP
    is_rtp_h264 = true;
  }
  g_print("Incoming pad %s: is_rtp_h264=%d is_raw_h264=%d (defaulted if unknown)\n", GST_PAD_NAME(pad), is_rtp_h264, is_raw_h264);

  // Choose a free video slot
  int slot_index = acquireFreeSlot();
  if (slot_index < 0) {
    g_printerr("No available video slots; ignoring new incoming pad %s\n", GST_PAD_NAME(pad));
    return;
  }

  // Insert an upstream leaky queue before depay/parse to absorb bursts
  GstElement* q_up = gst_element_factory_make("queue", nullptr);
  if (!q_up) { g_printerr("Failed to create upstream queue\n"); return; }
  g_object_set(G_OBJECT(q_up),
               "leaky", 2 /* downstream */,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  // Explicit H.264 receive chain: [rtph264depay?] -> h264parse -> vtdec/avdec_h264 -> queue(leaky) -> slot videoconvert
  GstElement* depay = is_rtp_h264 ? gst_element_factory_make("rtph264depay", nullptr) : nullptr;
  GstElement* parse = gst_element_factory_make("h264parse", nullptr);
  GstElement* dec = gst_element_factory_make("vtdec", nullptr);
  if (!dec) dec = gst_element_factory_make("avdec_h264", nullptr);
  GstElement* q_down = gst_element_factory_make("queue", nullptr);
  if ((!is_rtp_h264 && !is_raw_h264) || !parse || !dec || !q_down || (is_rtp_h264 && !depay)) {
    g_printerr("Failed to create H264 receive chain elements\n");
    return;
  }
  // Force parser to not passthrough and normalize to avc/au
  g_object_set(G_OBJECT(parse),
               "disable-passthrough", TRUE,
               NULL);
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(dec), "realtime")) {
    g_object_set(G_OBJECT(dec), "realtime", TRUE, NULL);
  }
  g_object_set(G_OBJECT(q_down),
               "leaky", 2 /* downstream */,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  if (is_rtp_h264) {
    gst_bin_add_many(GST_BIN(pipeline_), q_up, depay, parse, dec, q_down, NULL);
  } else {
    gst_bin_add_many(GST_BIN(pipeline_), q_up, parse, dec, q_down, NULL);
  }
  gst_element_sync_state_with_parent(q_up);
  if (is_rtp_h264) gst_element_sync_state_with_parent(depay);
  gst_element_sync_state_with_parent(parse);
  gst_element_sync_state_with_parent(dec);
  gst_element_sync_state_with_parent(q_down);

  // Link jitsibin pad -> queue
  {
    GstPad* qsink = gst_element_get_static_pad(q_up, "sink");
    if (!qsink) return;
    GstPadLinkReturn linkret = gst_pad_link(pad, qsink);
    gst_object_unref(qsink);
    if (linkret != GST_PAD_LINK_OK) {
      g_printerr("Failed to link jitsibin pad -> upstream queue\n");
      return;
    }
  }
  // Link: q_up -> [depay] -> parse
  if (is_rtp_h264) {
    if (!gst_element_link(q_up, depay)) {
      g_printerr("Failed to link q_up -> rtph264depay\n");
      return;
    }
    if (!gst_element_link(depay, parse)) {
      g_printerr("Failed to link rtph264depay -> h264parse\n");
      return;
    }
  } else {
    if (!gst_element_link(q_up, parse)) {
      g_printerr("Failed to link q_up -> h264parse (raw h264)\n");
      return;
    }
  }
  // Link parse -> dec with caps enforcing avc/au for decoder compatibility
  {
    GstCaps* dec_caps = gst_caps_new_simple("video/x-h264",
                                            "stream-format", G_TYPE_STRING, "avc",
                                            "alignment", G_TYPE_STRING, "au",
                                            NULL);
    if (!gst_element_link_filtered(parse, dec, dec_caps)) {
      g_printerr("Failed to link h264parse -> decoder with filtered caps (avc/au)\n");
      gst_caps_unref(dec_caps);
      return;
    }
    gst_caps_unref(dec_caps);
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
  g_print("H264 receive chain linked OK (slot %d) via %s\n",
          slot_index, is_rtp_h264 ? "RTP depay" : "raw h264");

  inUse_[slot_index] = true;

  char dot_name[128];
  g_snprintf(dot_name, sizeof(dot_name), "receive_%s", GST_PAD_NAME(pad));
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            dot_name);
}


