// ParticipantManager: encapsulates receive-slot setup and jitsibin pad handling
#pragma once

#include <gst/gst.h>

#include <vector>
#include <map>
#include <string>
#include <mutex>

class QQuickWindow;
class QQuickItem;
class ParticipantInfo;

class ParticipantManager {
public:
  ParticipantManager(GstElement* pipeline, GstElement* jitsibin);

  // Connect jitsibin signals; call before starting the pipeline state
  void connectSignals();

  // Discover QML slots (videoItem0..N) and create per-slot GL chains
  // Returns false if no slots found or setup fails
  bool initializeSlots(QQuickWindow* rootWindow);

  // Get the videoconvert element for a specific slot (for local preview)
  // Returns nullptr if slot index is out of range
  GstElement* getSlotVideoconvert(int slotIndex);

  // Reserve a slot for local preview (mark as in-use)
  void reserveSlot(int slotIndex);

  // Set participant info objects for updating UI (pass array of 4 ParticipantInfo pointers)
  void setParticipantInfoSlots(ParticipantInfo* slot0, ParticipantInfo* slot1, ParticipantInfo* slot2, ParticipantInfo* slot3);

private:
  // Static thunks for GObject signals
  static void onPadAdded(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data);
  static void onParticipantJoined(GstElement* /*jitsibin*/, const gchar* participant_id, const gchar* nick, gpointer user_data);
  static void onParticipantLeft(GstElement* /*jitsibin*/, const gchar* participant_id, const gchar* nick, gpointer user_data);
  static void onMuteStateChanged(GstElement* /*jitsibin*/, const gchar* participant_id, gboolean is_audio, gboolean new_muted, gpointer user_data);
  static void onFinished(GstElement* /*jitsibin*/, gboolean success, gpointer user_data);

  // Instance handlers
  void handlePadAdded(GstPad* pad);
  void handleVideoPadAdded(GstPad* pad, const std::string& padName, const std::string& codec);
  void handleAudioPadAdded(GstPad* pad, const std::string& padName);
  void handleParticipantJoined(const gchar* participant_id, const gchar* nick);
  void handleParticipantLeft(const gchar* participant_id, const gchar* nick);
  void handleMuteStateChanged(const gchar* participant_id, gboolean is_audio, gboolean new_muted);
  void handleFinished(gboolean success);

  // Helpers
  int acquireFreeSlotLocked();
  void teardownPad(GstPad* pad);
  void teardownAudioPad(GstPad* pad);
  std::string getParticipantIdFromPadName(const std::string& padName);
  std::string getCodecFromPadName(const std::string& padName);

private:
  GstElement* pipeline_;
  GstElement* jitsibin_;

  // Per-slot receive tail: videoconvert -> queue -> glupload -> glcolorconvert -> qml6glsink
  std::vector<GstElement*> videoconverts_;
  std::vector<GstElement*> queues_;
  std::vector<GstElement*> gluploads_;
  std::vector<GstElement*> glcolorconverts_;
  std::vector<GstElement*> sinks_;
  std::vector<QQuickItem*> videoItems_;
  std::vector<bool> inUse_;

  // Guards access to slot vectors and inUse_ during pad-added handling
  std::mutex slotMutex_;

  struct ReceiveSession {
    int slotIndex;
    GstElement* parser;
    GstElement* decoder;
  };
  std::map<GstPad*, ReceiveSession> activeSessions_;
  
  // Audio receive sessions: jitsibin pad -> audio chain elements
  struct AudioSession {
    GstElement* decoder;      // opusdec
    GstElement* convert;      // audioconvert
    GstElement* resample;     // audioresample
    GstElement* queue;        // queue (decoupling buffer)
    GstElement* sink;         // osxaudiosink
  };
  std::map<GstPad*, AudioSession> audioSessions_;

  std::map<std::string, std::vector<GstPad*>> participantPads_;

  // Participant info for each slot (owned by AppController, not owned here)
  std::vector<ParticipantInfo*> participantInfoSlots_;

  // Track which participant ID is in which slot for quick lookup
  std::map<std::string, int> participantIdToSlot_;
  std::map<int, std::string> slotToParticipantId_;

  // Store participant nicknames for display
  std::map<std::string, std::string> participantNicks_;
};


