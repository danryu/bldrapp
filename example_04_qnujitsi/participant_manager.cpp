#include "participant_manager.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/gstdebugutils.h>

#include <mutex>

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
    GstElement* queue = gst_element_factory_make("queue", nullptr);
    GstElement* glup = gst_element_factory_make("glupload", nullptr);
    GstElement* glcc = gst_element_factory_make("glcolorconvert", nullptr);
    GstElement* vsink = gst_element_factory_make("qml6glsink", nullptr);
    if (!vconv || !queue || !glup || !glcc || !vsink) {
      g_printerr("Failed to create receive elements for slot %d\n", i);
      return false;
    }
    
    // Configure queue for smooth frame delivery to GL pipeline
    // Use leaky downstream to drop old frames when queue fills, preventing artifacts
    g_object_set(G_OBJECT(queue),
                 "max-size-buffers", 5,           // Limit buffer count
                 "max-size-bytes", 0,             // Unlimited bytes
                 "max-size-time", 0,              // Unlimited time
                 "leaky", 2,                      // GST_QUEUE_LEAK_DOWNSTREAM - drop old frames
                 NULL);
    
    gst_bin_add_many(GST_BIN(pipeline_), vconv, queue, glup, glcc, vsink, NULL);
    if (!gst_element_link_many(vconv, queue, glup, glcc, vsink, NULL)) {
      g_printerr("Failed to link videoconvert -> queue -> glupload -> glcolorconvert -> qml6glsink for slot %d\n", i);
      return false;
    }
    
    // Key fix: Enable sync for proper frame timing, disable async to match osxvideosink behavior
    g_object_set(G_OBJECT(vsink),
                 "widget", item,
                 "sync", TRUE,   // Enable sync for proper frame timing (prevents artifacts)
                 "async", FALSE, // Match osxvideosink behavior
                 NULL);

    videoconverts_.push_back(vconv);
    queues_.push_back(queue);
    gluploads_.push_back(glup);
    glcolorconverts_.push_back(glcc);
    sinks_.push_back(vsink);
    inUse_.push_back(false);
  }
  g_print("Prepared %zu video slots\n", sinks_.size());
  return !sinks_.empty();
}

GstElement* ParticipantManager::getSlotVideoconvert(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= static_cast<int>(videoconverts_.size())) {
    return nullptr;
  }
  return videoconverts_[slotIndex];
}

void ParticipantManager::reserveSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= static_cast<int>(inUse_.size())) {
    return;
  }
  std::lock_guard<std::mutex> lock(slotMutex_);
  inUse_[slotIndex] = true;
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

int ParticipantManager::acquireFreeSlotLocked() {
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

  g_print("Incoming pad %s: assuming raw H.264\n", GST_PAD_NAME(pad));

  // Choose and reserve a free video slot under lock to avoid races between
  // concurrent pad-added callbacks that might otherwise pick the same slot.
  int slot_index = -1;
  GstElement* slot_videoconvert = nullptr;
  {
    std::lock_guard<std::mutex> lock(slotMutex_);
    for (size_t i = 0; i < inUse_.size(); ++i) {
      if (!inUse_[i]) {
        inUse_[i] = true;  // reserve immediately
        slot_index = static_cast<int>(i);
        slot_videoconvert = videoconverts_[i];
        break;
      }
    }
  }

  if (slot_index < 0) {
    g_printerr("No available video slots; ignoring new incoming pad %s\n", GST_PAD_NAME(pad));
    return;
  }

  // Simplified H.264 receive chain mirroring example_00a_qreceiver: h264parse -> vtdec -> videoconvert
  GstElement* parse = gst_element_factory_make("h264parse", nullptr);
  GstElement* dec = gst_element_factory_make("vtdec", nullptr);
  if (!dec) dec = gst_element_factory_make("avdec_h264", nullptr);
  
  if (!parse || !dec) {
    g_printerr("Failed to create H264 receive chain elements\n");
    return;
  }

  // Use GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT flag as in example_00a_qreceiver
  g_object_set(G_OBJECT(dec),
               "automatic-request-sync-points", TRUE,
               "automatic-request-sync-point-flags", GST_VIDEO_DECODER_REQUEST_SYNC_POINT_CORRUPT_OUTPUT,
               NULL);

  gst_bin_add_many(GST_BIN(pipeline_), parse, dec, NULL);

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

  // Link h264parse -> decoder -> videoconvert (simplified, no queues or caps filter)
  if (!gst_element_link(parse, dec)) {
    g_printerr("Failed to link h264parse -> decoder\n");
    {
      std::lock_guard<std::mutex> lock(slotMutex_);
      inUse_[slot_index] = false;
    }
    return;
  }
  if (!gst_element_link(dec, slot_videoconvert)) {
    g_printerr("Failed to link decoder -> videoconvert (slot %d)\n", slot_index);
    {
      std::lock_guard<std::mutex> lock(slotMutex_);
      inUse_[slot_index] = false;
    }
    return;
  }

  // Sync state in reverse order as in example_00a_qreceiver
  gst_element_sync_state_with_parent(videoconverts_[slot_index]);
  gst_element_sync_state_with_parent(queues_[slot_index]);
  gst_element_sync_state_with_parent(gluploads_[slot_index]);
  gst_element_sync_state_with_parent(glcolorconverts_[slot_index]);
  gst_element_sync_state_with_parent(sinks_[slot_index]);
  gst_element_sync_state_with_parent(dec);
  gst_element_sync_state_with_parent(parse);


  g_print("H264 receive chain linked OK (slot %d) - simplified pipeline\n", slot_index);

  inUse_[slot_index] = true;

  char dot_name[128];
  g_snprintf(dot_name, sizeof(dot_name), "receive_%s", GST_PAD_NAME(pad));
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            dot_name);
}


