// Conference: orchestrates pipeline, jitsibin, send and participant management
#pragma once

#include <gst/gst.h>

class QQuickWindow;

class SendPipeline;
class ParticipantManager;

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
  bool initQmlSlotsAndSend(QQuickWindow* rootWindow, int videoWidth, int videoHeight);

  // Dump a GST dot graph
  void dumpDot(const char* name) const;

  // Schedule PLAYING on the Qt render thread
  void scheduleStart(QQuickWindow* window) const;

  // Accessors for cleanup
  GstElement* pipeline() const { return pipeline_; }

private:
  GstElement* pipeline_ {nullptr};
  GstElement* jitsibin_ {nullptr};
  SendPipeline* send_ {nullptr};
  ParticipantManager* participants_ {nullptr};
};


