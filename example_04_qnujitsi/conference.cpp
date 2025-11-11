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
               "force-play", TRUE,
               "insecure", TRUE,
               NULL);

  gst_bin_add_many(GST_BIN(pipeline_), jitsibin_, NULL);

  send_ = new SendPipeline(pipeline_, jitsibin_);
  if (!send_->buildAndLink(videoWidth, videoHeight)) {
    g_printerr("Failed to build send pipeline\n");
    return false;
  }

  participants_ = new ParticipantManager(pipeline_, jitsibin_);
  participants_->connectSignals();

  return true;
}

bool Conference::initQmlSlots(QQuickWindow* rootWindow) {
  if (!participants_) return false;
  return participants_->initializeSlots(rootWindow);
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


