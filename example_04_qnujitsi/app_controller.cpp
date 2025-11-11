#include "app_controller.h"

#include <QQuickWindow>
#include <QString>

#include <gst/gst.h>

#include "conference.h"

AppController::AppController(QObject* parent)
  : QObject(parent) {
  startGlibLoop();
}

AppController::~AppController() {
  disconnect();
  stopGlibLoop();
}

void AppController::startGlibLoop() {
  glibLoop_ = g_main_loop_new(nullptr, FALSE);
  glibThread_ = std::thread([this]{
    g_main_loop_run(glibLoop_);
  });
}

void AppController::stopGlibLoop() {
  if (glibLoop_) {
    g_main_loop_quit(glibLoop_);
    if (glibThread_.joinable()) glibThread_.join();
    g_main_loop_unref(glibLoop_);
    glibLoop_ = nullptr;
  }
}

void AppController::setRoot(QObject* windowObject) {
  root_ = qobject_cast<QQuickWindow*>(windowObject);
}

void AppController::connectTo(const QString& host) {
  if (!root_) return;
  // If already connected, disconnect first
  if (conf_) {
    disconnect();
  }
  conf_ = new Conference();
  if (!conf_->build(host.toUtf8().constData(),
                    "video",
                    videoWidth_,
                    videoHeight_,
                    receiveLimit_,
                    receiveMaxHeight_)) {
    delete conf_;
    conf_ = nullptr;
    return;
  }
  if (!conf_->initQmlSlots(root_)) {
    delete conf_;
    conf_ = nullptr;
    return;
  }
  conf_->dumpDot("qnujitsi");
  conf_->scheduleStart(root_);
}

void AppController::disconnect() {
  if (!conf_) return;
  gst_element_set_state(conf_->pipeline(), GST_STATE_NULL);
  gst_object_unref(conf_->pipeline());
  delete conf_;
  conf_ = nullptr;
}


