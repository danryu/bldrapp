#include "conference.h"

#include <QRunnable>
#include <QQuickWindow>

#include <gst/gst.h>
#include <gst/gstdebugutils.h>

#include "send_pipeline.h"
#include "participant_manager.h"

class SetPlayingRenderJob : public QRunnable {
public:
  explicit SetPlayingRenderJob(GstElement* pipeline)
    : pipeline_(pipeline ? GST_ELEMENT(gst_object_ref(pipeline)) : nullptr) {}
  ~SetPlayingRenderJob() override {
    if (pipeline_) gst_object_unref(pipeline_);
  }
  void run() override {
    if (pipeline_) gst_element_set_state(pipeline_, GST_STATE_PLAYING);
  }
private:
  GstElement* pipeline_;
};

Conference::Conference() = default;

Conference::~Conference() {
  delete participants_;
  delete send_;
  // pipeline cleanup is done by caller to preserve current lifecycle
}

bool Conference::build(const char* host,
                       const char* room,
                       int videoWidth,
                       int videoHeight,
                       int receiveLimit,
                       int receiveMaxHeight) {
  pipeline_ = gst_pipeline_new(nullptr);
  jitsibin_ = gst_element_factory_make("jitsibin", nullptr);
  if (!pipeline_ || !jitsibin_) {
    g_printerr("Failed to create pipeline or jitsibin\n");
    return false;
  }

  g_object_set(G_OBJECT(jitsibin_),
               "server", host,
               "room", room,
               "nick", "qnujitsi_user",
               "video-codec", 1, /* H264 enum value */
               "receive-limit", receiveLimit,
               "receive-max-height", receiveMaxHeight,
               "jitterbuffer-latency", 300, /* 300ms jitter buffer */
               "force-play", TRUE,
               "insecure", TRUE,
               NULL);

  gst_bin_add_many(GST_BIN(pipeline_), jitsibin_, NULL);

  participants_ = new ParticipantManager(pipeline_, jitsibin_);
  participants_->connectSignals();

  return true;
}

bool Conference::initQmlSlotsAndSend(QQuickWindow* rootWindow, int videoWidth, int videoHeight) {
  if (!participants_) return false;

  // Initialize QML slots first
  if (!participants_->initializeSlots(rootWindow)) {
    return false;
  }

  // Get slot 0's videoconvert for local preview and reserve it
  GstElement* localSlotVideoconvert = participants_->getSlotVideoconvert(0);
  if (!localSlotVideoconvert) {
    g_printerr("Failed to get slot 0 videoconvert for local preview\n");
    return false;
  }

  // Reserve slot 0 for local preview
  participants_->reserveSlot(0);

  // Build send pipeline with local preview to slot 0
  send_ = new SendPipeline(pipeline_, jitsibin_);
  if (!send_->buildAndLink(videoWidth, videoHeight, localSlotVideoconvert)) {
    g_printerr("Failed to build send pipeline with local preview\n");
    return false;
  }

  g_print("Local preview configured to slot 0\n");
  return true;
}

void Conference::dumpDot(const char* name) const {
  if (!pipeline_) return;
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline_),
                            GST_DEBUG_GRAPH_SHOW_ALL,
                            name ? name : "pipeline");
}

void Conference::scheduleStart(QQuickWindow* window) const {
  if (!window || !pipeline_) return;
  window->scheduleRenderJob(new SetPlayingRenderJob(pipeline_), QQuickWindow::BeforeSynchronizingStage);
}


