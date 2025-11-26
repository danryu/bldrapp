#include "app_controller.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QString>

#include <gst/gst.h>

#include "conference.h"
#include "camera_manager.h"

AppController::AppController(QObject* parent)
  : QObject(parent) {
  // Initialize camera manager and enumerate cameras
  cameraManager_ = std::make_unique<CameraManager>(this);
  cameraManager_->enumerateCameras();

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

  // Get selected camera device
  const char* cameraDeviceIndex = nullptr;
  if (cameraManager_) {
    const CameraDevice* selectedCamera = cameraManager_->selectedCamera();
    if (selectedCamera) {
      cameraDeviceIndex = selectedCamera->deviceName.c_str();
      qDebug() << "Using camera:" << QString::fromStdString(selectedCamera->displayName)
               << "with device index:" << cameraDeviceIndex;
    }
  }

  if (!conference_->initQmlSlotsAndSend(rootWindow, videoWidth, videoHeight, cameraDeviceIndex)) {
    conference_.reset();
    emit error(QStringLiteral("Failed to initialize QML slots and send pipeline"));
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
  
  // Reset conference first to destroy participants and send pipeline
  // while the pipeline object is still valid.
  conference_.reset();
  
  if (pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
  }
  emit connectedChanged();
}


