#include "participant_manager.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QMetaObject>

#include <gst/gst.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/gstdebugutils.h>

#include <mutex>
#include <algorithm>

ParticipantManager::ParticipantManager(GstElement* pipeline, GstElement* jitsibin)
  : pipeline_(pipeline), jitsibin_(jitsibin) {}

void ParticipantManager::connectSignals() {
  g_signal_connect(jitsibin_, "pad-added", GCallback(&ParticipantManager::onPadAdded), this);
  g_signal_connect(jitsibin_, "participant-joined", GCallback(&ParticipantManager::onParticipantJoined), this);
  g_signal_connect(jitsibin_, "participant-left", GCallback(&ParticipantManager::onParticipantLeft), this);
  g_signal_connect(jitsibin_, "mute-state-changed", GCallback(&ParticipantManager::onMuteStateChanged), this);
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
    
    // Configure sink differently for slot 0 (local preview) vs remote slots
    if (i == 0) {
      // Slot 0 (local preview): disable sync for immediate frame display from live camera
      g_object_set(G_OBJECT(vsink),
                   "widget", item,
                   "sync", FALSE,  // No sync for local preview - display frames immediately
                   "async", FALSE, // Match osxvideosink behavior
                   "max-lateness", -1, // Never drop frames due to lateness
                   "qos", FALSE,   // Disable QoS to prevent frame drops
                   NULL);
    } else {
      // Slots 1-3 (remote participants): enable sync for proper playback timing
      g_object_set(G_OBJECT(vsink),
                   "widget", item,
                   "sync", TRUE,   // Enable sync for proper frame timing (prevents artifacts)
                   "async", FALSE, // Match osxvideosink behavior
                   NULL);
    }

    videoconverts_.push_back(vconv);
    queues_.push_back(queue);
    gluploads_.push_back(glup);
    glcolorconverts_.push_back(glcc);
    sinks_.push_back(vsink);
    videoItems_.push_back(item);
    inUse_.push_back(false);

    // Initial state: only local preview (slot 0) is visible
    item->setVisible(i == 0);
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

void ParticipantManager::onParticipantJoined(GstElement* /*jitsibin*/, const gchar* participant_id, const gchar* nick, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self) return;
  self->handleParticipantJoined(participant_id, nick);
}

void ParticipantManager::onParticipantLeft(GstElement* /*jitsibin*/, const gchar* participant_id, const gchar* nick, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self) return;
  self->handleParticipantLeft(participant_id, nick);
}

void ParticipantManager::onMuteStateChanged(GstElement* /*jitsibin*/, const gchar* participant_id, gboolean is_audio, gboolean new_muted, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self) return;
  self->handleMuteStateChanged(participant_id, is_audio, new_muted);
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

std::string ParticipantManager::getParticipantIdFromPadName(const std::string& padName) {
  // Format: participantId_codec_ssrc
  // Search from right
  auto i = padName.rfind('_');
  if (i == std::string::npos) return "";
  
  auto j = padName.rfind('_', i - 1);
  if (j == std::string::npos) return "";
  
  return padName.substr(0, j);
}

void ParticipantManager::teardownPad(GstPad* pad) {
  std::lock_guard<std::mutex> lock(slotMutex_);
  auto it = activeSessions_.find(pad);
  if (it == activeSessions_.end()) {
    // Could be already torn down
    return;
  }

  const auto& session = it->second;
  int slotIndex = session.slotIndex;
  g_print("  Cleaning up slot %d (parser=%p, decoder=%p)\n", slotIndex, session.parser, session.decoder);

  // 1. Unlink decoder -> videoconvert (the slot's permanent input)
  GstElement* slotVideoconvert = videoconverts_[slotIndex];
  if (session.decoder && slotVideoconvert) {
      gst_element_unlink(session.decoder, slotVideoconvert);
  }

  // 2. Set elements to NULL and remove from pipeline
  if (session.decoder) {
    gst_element_set_state(session.decoder, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline_), session.decoder);
  }
  if (session.parser) {
    gst_element_set_state(session.parser, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(pipeline_), session.parser);
  }

  // 3. Mark slot as free
  inUse_[slotIndex] = false;
  
  // Hide the video item on the main thread
  if (slotIndex < static_cast<int>(videoItems_.size())) {
    QQuickItem* item = videoItems_[slotIndex];
    if (item) {
       QMetaObject::invokeMethod(item, [item](){ item->setVisible(false); }, Qt::QueuedConnection);
    }
  }

  activeSessions_.erase(it);
  g_print("  Slot %d freed.\n", slotIndex);
}

void ParticipantManager::handleParticipantJoined(const gchar* participant_id, const gchar* nick) {
  g_print("ParticipantManager::handleParticipantJoined id=%s nick=%s\n", participant_id, nick);
}

void ParticipantManager::handleParticipantLeft(const gchar* participant_id, const gchar* nick) {
  g_print("ParticipantManager::handleParticipantLeft id=%s nick=%s\n", participant_id, nick);
  
  std::string pid(participant_id);
  std::vector<GstPad*> padsToTeardown;
  
  {
    std::lock_guard<std::mutex> lock(slotMutex_);
    auto it = participantPads_.find(pid);
    if (it != participantPads_.end()) {
      padsToTeardown = it->second;
      // We will clear the entry now, as we are tearing them down
      participantPads_.erase(it);
    }
  }
  
  if (padsToTeardown.empty()) {
    g_print("  No tracked pads for participant %s\n", participant_id);
    return;
  }

  for (GstPad* pad : padsToTeardown) {
     g_print("  Tearing down pad for participant %s\n", participant_id);
     // Drop lock before calling teardownPad because it acquires lock
     teardownPad(pad);
  }
}

void ParticipantManager::handleMuteStateChanged(const gchar* participant_id, gboolean is_audio, gboolean new_muted) {
  g_print("ParticipantManager::handleMuteStateChanged id=%s %s=%d\n", participant_id, is_audio ? "audio" : "video", new_muted);
}

void ParticipantManager::handlePadAdded(GstPad* pad) {
  if (!pipeline_) return;

  gchar* padNameC = gst_object_get_name(GST_OBJECT(pad));
  std::string padName = padNameC ? std::string(padNameC) : "";
  if (padNameC) g_free(padNameC);

  g_print("Incoming pad %s: assuming raw H.264\n", padName.c_str());

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

  {
    std::lock_guard<std::mutex> lock(slotMutex_);
    inUse_[slot_index] = true;
    activeSessions_[pad] = {slot_index, parse, dec};

    std::string pid = getParticipantIdFromPadName(padName);
    if (!pid.empty()) {
      participantPads_[pid].push_back(pad);
      g_print("  Mapped pad to participant %s\n", pid.c_str());
    } else {
      g_print("  Could not parse participant ID from pad name %s\n", padName.c_str());
    }

    // Show the video item on the main thread
    if (slot_index < static_cast<int>(videoItems_.size())) {
      QQuickItem* item = videoItems_[slot_index];
      if (item) {
         QMetaObject::invokeMethod(item, [item](){ item->setVisible(true); }, Qt::QueuedConnection);
      }
    }
  }

  char dot_name[128];
  g_snprintf(dot_name, sizeof(dot_name), "receive_%s", GST_PAD_NAME(pad));
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            dot_name);
}
