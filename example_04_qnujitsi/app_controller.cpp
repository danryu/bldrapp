#include "app_controller.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QString>

#include <gst/gst.h>

#include "conference.h"

AppController::AppController(QObject* parent)
  : QObject(parent) {
  // Ensure graceful teardown on app exit
  if (QCoreApplication::instance()) {
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
      teardown();
    });
  }
}

AppController::~AppController() {
  teardown();
}

bool AppController::connectToConference(QQuickWindow* rootWindow,
                                        const QString& host,
                                        const QString& room,
                                        int videoWidth,
                                        int videoHeight,
                                        int receiveLimit,
                                        int receiveMaxHeight) {
  if (!rootWindow) {
    emit error(QStringLiteral("Root window is null"));
    return false;
  }
  if (host.isEmpty() || room.isEmpty()) {
    emit error(QStringLiteral("Host and room are required"));
    return false;
  }
  if (conference_) {
    emit error(QStringLiteral("Conference already connected"));
    return false;
  }

  conference_ = std::make_unique<Conference>();
  if (!conference_->build(host.toUtf8().constData(),
                          room.toUtf8().constData(),
                          videoWidth,
                          videoHeight,
                          receiveLimit,
                          receiveMaxHeight)) {
    conference_.reset();
    emit error(QStringLiteral("Failed to build conference"));
    return false;
  }

  if (!conference_->initQmlSlots(rootWindow)) {
    conference_.reset();
    emit error(QStringLiteral("Failed to initialize QML slots"));
    return false;
  }

  conference_->dumpDot("qnujitsi");
  conference_->scheduleStart(rootWindow);

  emit connectedChanged();
  return true;
}

void AppController::disconnectConference() {
  teardown();
}

bool AppController::isConnected() const {
  return static_cast<bool>(conference_);
}

void AppController::teardown() {
  if (!conference_) return;
  GstElement* pipeline = conference_->pipeline();
  if (pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
  }
  conference_.reset();
  emit connectedChanged();
}


