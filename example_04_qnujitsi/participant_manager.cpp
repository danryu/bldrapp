#include "participant_manager.h"
#include "participant_info.h"

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

void ParticipantManager::disconnectSignals() {
  // Set shutdown flag first - signal handlers will check this and return early
  shuttingDown_.store(true, std::memory_order_release);

  // Disconnect all signals from jitsibin to prevent further callbacks
  if (jitsibin_) {
    g_signal_handlers_disconnect_by_data(jitsibin_, this);
  }
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
      // Slots 1-15 (remote participants): enable sync for proper playback timing
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

void ParticipantManager::setParticipantInfoSlots(ParticipantInfo* slot0, ParticipantInfo* slot1, ParticipantInfo* slot2, ParticipantInfo* slot3,
                                                   ParticipantInfo* slot4, ParticipantInfo* slot5, ParticipantInfo* slot6, ParticipantInfo* slot7,
                                                   ParticipantInfo* slot8, ParticipantInfo* slot9, ParticipantInfo* slot10, ParticipantInfo* slot11,
                                                   ParticipantInfo* slot12, ParticipantInfo* slot13, ParticipantInfo* slot14, ParticipantInfo* slot15) {
  participantInfoSlots_ = {slot0, slot1, slot2, slot3, slot4, slot5, slot6, slot7,
                            slot8, slot9, slot10, slot11, slot12, slot13, slot14, slot15};
}

void ParticipantManager::onPadAdded(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self || self->shuttingDown_.load(std::memory_order_acquire)) return;
  self->handlePadAdded(pad);
}

void ParticipantManager::onParticipantJoined(GstElement* /*jitsibin*/, const gchar* participant_id, const gchar* nick, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self || self->shuttingDown_.load(std::memory_order_acquire)) return;
  self->handleParticipantJoined(participant_id, nick);
}

void ParticipantManager::onParticipantLeft(GstElement* /*jitsibin*/, const gchar* participant_id, const gchar* nick, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self || self->shuttingDown_.load(std::memory_order_acquire)) return;
  self->handleParticipantLeft(participant_id, nick);
}

void ParticipantManager::onMuteStateChanged(GstElement* /*jitsibin*/, const gchar* participant_id, gboolean is_audio, gboolean new_muted, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self || self->shuttingDown_.load(std::memory_order_acquire)) return;
  self->handleMuteStateChanged(participant_id, is_audio, new_muted);
}

void ParticipantManager::onFinished(GstElement* /*jitsibin*/, gboolean success, gpointer user_data) {
  auto* self = static_cast<ParticipantManager*>(user_data);
  if (!self || self->shuttingDown_.load(std::memory_order_acquire)) return;
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

std::string ParticipantManager::getCodecFromPadName(const std::string& padName) {
  // Format: participantId_CODEC_ssrc
  // Search from right to find the codec between the two underscores
  auto i = padName.rfind('_');
  if (i == std::string::npos || i == 0) return "";
  
  auto j = padName.rfind('_', i - 1);
  if (j == std::string::npos) return "";
  
  return padName.substr(j + 1, i - j - 1);
}

void ParticipantManager::teardownPad(GstPad* pad) {
  std::lock_guard<std::mutex> lock(slotMutex_);
  auto it = activeSessions_.find(pad);
  if (it == activeSessions_.end()) {
    // Could be already torn down or might be an audio pad
    return;
  }

  const auto& session = it->second;
  int slotIndex = session.slotIndex;
  g_print("  Cleaning up video slot %d (parser=%p, decoder=%p)\n", slotIndex, session.parser, session.decoder);

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
  g_print("  Video slot %d freed.\n", slotIndex);
}

void ParticipantManager::teardownAudioPad(GstPad* pad) {
  std::lock_guard<std::mutex> lock(slotMutex_);
  auto it = audioSessions_.find(pad);
  if (it == audioSessions_.end()) {
    return;
  }

  const auto& session = it->second;
  g_print("  Cleaning up audio session (decoder=%p, convert=%p, resample=%p, queue=%p, sink=%p)\n",
          session.decoder, session.convert, session.resample, session.queue, session.sink);

  // Set elements to NULL and remove from pipeline (downstream to upstream order)
  auto removeElement = [this](GstElement* elem) {
    if (elem) {
      gst_element_set_state(elem, GST_STATE_NULL);
      gst_bin_remove(GST_BIN(pipeline_), elem);
    }
  };

  removeElement(session.sink);
  removeElement(session.queue);
  removeElement(session.resample);
  removeElement(session.convert);
  removeElement(session.decoder);

  audioSessions_.erase(it);
  g_print("  Audio session freed.\n");
}

void ParticipantManager::handleParticipantJoined(const gchar* participant_id, const gchar* nick) {
  g_print("ParticipantManager::handleParticipantJoined id=%s nick=%s\n", participant_id, nick);

  std::lock_guard<std::mutex> lock(slotMutex_);
  participantNicks_[std::string(participant_id)] = nick ? std::string(nick) : std::string(participant_id);
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

    // Clear participant info if they have a slot assigned
    auto slotIt = participantIdToSlot_.find(pid);
    if (slotIt != participantIdToSlot_.end()) {
      int slotIndex = slotIt->second;
      if (slotIndex >= 0 && slotIndex < static_cast<int>(participantInfoSlots_.size()) && participantInfoSlots_[slotIndex]) {
        QMetaObject::invokeMethod(participantInfoSlots_[slotIndex], [info = participantInfoSlots_[slotIndex]](){
          info->reset();
        }, Qt::QueuedConnection);
      }
      slotToParticipantId_.erase(slotIndex);
      participantIdToSlot_.erase(slotIt);
    }

    // Remove nickname
    participantNicks_.erase(pid);
  }

  if (padsToTeardown.empty()) {
    g_print("  No tracked pads for participant %s\n", participant_id);
    return;
  }

  for (GstPad* pad : padsToTeardown) {
     g_print("  Tearing down pad for participant %s\n", participant_id);
     // Try video teardown first, then audio
     // Note: teardownPad and teardownAudioPad acquire their own locks
     teardownPad(pad);
     teardownAudioPad(pad);
  }
}

void ParticipantManager::handleMuteStateChanged(const gchar* participant_id, gboolean is_audio, gboolean new_muted) {
  g_print("ParticipantManager::handleMuteStateChanged id=%s %s=%d\n", participant_id, is_audio ? "audio" : "video", new_muted);

  std::lock_guard<std::mutex> lock(slotMutex_);
  std::string pid(participant_id);
  auto slotIt = participantIdToSlot_.find(pid);
  if (slotIt != participantIdToSlot_.end()) {
    int slotIndex = slotIt->second;
    if (slotIndex >= 0 && slotIndex < static_cast<int>(participantInfoSlots_.size()) && participantInfoSlots_[slotIndex]) {
      ParticipantInfo* info = participantInfoSlots_[slotIndex];
      bool muted = new_muted != 0;
      if (is_audio) {
        QMetaObject::invokeMethod(info, [info, muted](){
          info->setAudioMuted(muted);
        }, Qt::QueuedConnection);
      } else {
        QMetaObject::invokeMethod(info, [info, muted](){
          info->setVideoMuted(muted);
        }, Qt::QueuedConnection);
      }
    }
  }
}

void ParticipantManager::handlePadAdded(GstPad* pad) {
  if (!pipeline_) return;

  gchar* padNameC = gst_object_get_name(GST_OBJECT(pad));
  std::string padName = padNameC ? std::string(padNameC) : "";
  if (padNameC) g_free(padNameC);

  // Parse codec from pad name (format: participantId_CODEC_ssrc)
  std::string codec = getCodecFromPadName(padName);
  g_print("Incoming pad %s: codec=%s\n", padName.c_str(), codec.c_str());

  // Route to appropriate handler based on codec
  if (codec == "OPUS") {
    handleAudioPadAdded(pad, padName);
  } else if (codec == "H264" || codec == "VP8" || codec == "VP9" || codec.empty()) {
    // Treat unknown/empty as H264 for backwards compatibility
    handleVideoPadAdded(pad, padName, codec.empty() ? "H264" : codec);
  } else {
    g_printerr("Unknown codec %s in pad %s, ignoring\n", codec.c_str(), padName.c_str());
  }
}

void ParticipantManager::handleAudioPadAdded(GstPad* pad, const std::string& padName) {
  g_print("Setting up audio receive chain for pad %s\n", padName.c_str());

  // Create audio receive chain: opusdec -> audioconvert -> audioresample -> queue -> osxaudiosink
  // The queue decouples audio processing from the rest of the pipeline, preventing
  // clock/timing disruption when audio is added to an already-running pipeline.
  GstElement* dec = gst_element_factory_make("opusdec", nullptr);
  GstElement* conv = gst_element_factory_make("audioconvert", nullptr);
  GstElement* resample = gst_element_factory_make("audioresample", nullptr);
  GstElement* queue = gst_element_factory_make("queue", nullptr);
  GstElement* sink = gst_element_factory_make("osxaudiosink", nullptr);  // Direct sink, not autoaudiosink

  if (!dec || !conv || !resample || !queue || !sink) {
    g_printerr("Failed to create audio receive chain elements\n");
    if (dec) gst_object_unref(dec);
    if (conv) gst_object_unref(conv);
    if (resample) gst_object_unref(resample);
    if (queue) gst_object_unref(queue);
    if (sink) gst_object_unref(sink);
    return;
  }

  // Configure queue for low-latency buffering
  g_object_set(G_OBJECT(queue),
               "max-size-buffers", 2,
               "max-size-bytes", 0,
               "max-size-time", 0,
               "leaky", 2,  // GST_QUEUE_LEAK_DOWNSTREAM - drop old if full
               NULL);

  // Configure sink for real-time playback without clock sync
  // CRITICAL: sync=FALSE prevents audio sink from affecting pipeline clock,
  // which would otherwise cause video frame rate to drop significantly.
  // In real-time conferencing, jitsibin's jitter buffer handles timing.
  g_object_set(G_OBJECT(sink),
               "sync", FALSE,   // Play audio immediately without clock sync
               "async", FALSE,  // Don't affect pipeline state transitions
               NULL);

  gst_bin_add_many(GST_BIN(pipeline_), dec, conv, resample, queue, sink, NULL);

  // Link audio chain FIRST (before linking to jitsibin pad)
  if (!gst_element_link_many(dec, conv, resample, queue, sink, NULL)) {
    g_printerr("Failed to link audio receive chain\n");
    return;
  }

  // Sync states upstream-to-downstream order BEFORE linking source pad
  // This ensures elements are ready to receive data
  gst_element_sync_state_with_parent(dec);
  gst_element_sync_state_with_parent(conv);
  gst_element_sync_state_with_parent(resample);
  gst_element_sync_state_with_parent(queue);
  gst_element_sync_state_with_parent(sink);

  // NOW link jitsibin pad -> opusdec (after chain is ready)
  {
    GstPad* sinkpad = gst_element_get_static_pad(dec, "sink");
    if (!sinkpad) {
      g_printerr("Failed to get opusdec sink pad\n");
      return;
    }
    GstPadLinkReturn linkret = gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
    if (linkret != GST_PAD_LINK_OK) {
      g_printerr("Failed to link jitsibin pad -> opusdec: %d\n", linkret);
      return;
    }
  }

  g_print("Audio receive chain linked OK for pad %s\n", padName.c_str());

  {
    std::lock_guard<std::mutex> lock(slotMutex_);
    audioSessions_[pad] = {dec, conv, resample, queue, sink};

    std::string pid = getParticipantIdFromPadName(padName);
    if (!pid.empty()) {
      participantPads_[pid].push_back(pad);
      g_print("  Mapped audio pad to participant %s\n", pid.c_str());
    }
  }
}

void ParticipantManager::handleVideoPadAdded(GstPad* pad, const std::string& padName, const std::string& codec) {
  g_print("Setting up video receive chain for pad %s (codec=%s)\n", padName.c_str(), codec.c_str());

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

      // Track slot assignment for this participant
      participantIdToSlot_[pid] = slot_index;
      slotToParticipantId_[slot_index] = pid;

      // Populate ParticipantInfo for this slot
      if (slot_index >= 0 && slot_index < static_cast<int>(participantInfoSlots_.size()) && participantInfoSlots_[slot_index]) {
        ParticipantInfo* info = participantInfoSlots_[slot_index];

        // Look up participant nickname
        std::string nick = pid;  // Default to participant ID
        auto nickIt = participantNicks_.find(pid);
        if (nickIt != participantNicks_.end()) {
          nick = nickIt->second;
        }

        // Update ParticipantInfo on Qt thread
        QMetaObject::invokeMethod(info, [info, pid, nick](){
          info->setParticipantId(QString::fromStdString(pid));
          info->setName(QString::fromStdString(nick));
          info->setIsActive(true);
        }, Qt::QueuedConnection);

        g_print("  Set participant info for slot %d: %s\n", slot_index, nick.c_str());
      }
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
