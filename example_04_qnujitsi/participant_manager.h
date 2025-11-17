// ParticipantManager: encapsulates receive-slot setup and jitsibin pad handling
#pragma once

#include <gst/gst.h>

#include <vector>

class QQuickWindow;
class QQuickItem;

class ParticipantManager {
public:
  ParticipantManager(GstElement* pipeline, GstElement* jitsibin);

  // Connect jitsibin signals; call before starting the pipeline state
  void connectSignals();

  // Discover QML slots (videoItem0..N) and create per-slot GL chains
  // Returns false if no slots found or setup fails
  bool initializeSlots(QQuickWindow* rootWindow);

private:
  // Static thunks for GObject signals
  static void onPadAdded(GstElement* /*jitsibin*/, GstPad* pad, gpointer user_data);
  static void onFinished(GstElement* /*jitsibin*/, gboolean success, gpointer user_data);

  // Instance handlers
  void handlePadAdded(GstPad* pad);
  void handleFinished(gboolean success);

  // Helpers
  int acquireFreeSlot() const;

private:
  GstElement* pipeline_;
  GstElement* jitsibin_;

  // Per-slot receive tail: videoconvert -> queue -> glupload -> glcolorconvert -> qml6glsink
  std::vector<GstElement*> videoconverts_;
  std::vector<GstElement*> queues_;
  std::vector<GstElement*> gluploads_;
  std::vector<GstElement*> glcolorconverts_;
  std::vector<GstElement*> sinks_;
  std::vector<bool> inUse_;
};


