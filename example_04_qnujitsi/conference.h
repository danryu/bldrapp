// Conference: orchestrates pipeline, jitsibin, send and participant management
#pragma once

#include <gst/gst.h>

class QQuickWindow;

class SendPipeline;
class ParticipantManager;
class ParticipantInfo;

class Conference {
public:
  Conference();
  ~Conference();

  // Build base pipeline and jitsibin, configure properties.
  bool build(const char* host,
             const char* room,
             int videoWidth,
             int videoHeight,
             int receiveLimit,
             int receiveMaxHeight);

  // Initialize per-slot QML sinks via ParticipantManager, then build send pipeline.
  // This order allows send pipeline to use slot 0 for local preview.
  // cameraDevice: GstDevice for camera, nullptr for test source
  // audioDeviceIndex: audio device index (e.g., "0", "1"), nullptr for default device
  bool initQmlSlotsAndSend(QQuickWindow* rootWindow, int videoWidth, int videoHeight,
                           GstDevice* cameraDevice = nullptr,
                           const char* audioDeviceIndex = nullptr);

  // Dump a GST dot graph
  void dumpDot(const char* name) const;

  // Schedule PLAYING on the Qt render thread
  void scheduleStart(QQuickWindow* window) const;

  // Mute controls
  bool setVideoMuted(bool muted);
  bool setAudioMuted(bool muted);
  bool isVideoMuted() const;
  bool isAudioMuted() const;

  // Set participant info objects for each slot (pass to ParticipantManager)
  void setParticipantInfoSlots(ParticipantInfo* slot0, ParticipantInfo* slot1, ParticipantInfo* slot2, ParticipantInfo* slot3);

  // Disconnect all signals before pipeline teardown to prevent use-after-free
  void disconnectSignals();

  // Accessors for cleanup
  GstElement* pipeline() const { return pipeline_; }

private:
  GstElement* pipeline_ {nullptr};
  GstElement* jitsibin_ {nullptr};
  SendPipeline* send_ {nullptr};
  ParticipantManager* participants_ {nullptr};
};


