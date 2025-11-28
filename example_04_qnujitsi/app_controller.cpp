#include "app_controller.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QString>

#include <gst/gst.h>

#include "conference.h"
#include "camera_manager.h"
#include "audio_manager.h"
#include "participant_info.h"

AppController::AppController(QObject* parent)
  : QObject(parent) {
  // Initialize camera manager and enumerate cameras
  cameraManager_ = std::make_unique<CameraManager>(this);
  cameraManager_->enumerateCameras();

  // Initialize audio manager and enumerate audio devices
  audioManager_ = std::make_unique<AudioManager>(this);
  audioManager_->enumerateAudioDevices();

  // Initialize participant info for each slot
  slot0Info_ = std::make_unique<ParticipantInfo>(this);
  slot1Info_ = std::make_unique<ParticipantInfo>(this);
  slot2Info_ = std::make_unique<ParticipantInfo>(this);
  slot3Info_ = std::make_unique<ParticipantInfo>(this);

  // Slot 0 is always local preview
  slot0Info_->setName("Local");
  slot0Info_->setIsActive(true);

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

  // Set participant info slots for UI updates
  conference_->setParticipantInfoSlots(slot0Info_.get(), slot1Info_.get(), slot2Info_.get(), slot3Info_.get());

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

  // Get selected audio device
  const char* audioDeviceIndex = nullptr;
  if (audioManager_) {
    const AudioDevice* selectedAudio = audioManager_->selectedAudioDevice();
    if (selectedAudio) {
      audioDeviceIndex = selectedAudio->deviceName.c_str();
      qDebug() << "Using audio device:" << QString::fromStdString(selectedAudio->displayName)
               << "with device index:" << audioDeviceIndex;
    }
  }

  if (!conference_->initQmlSlotsAndSend(rootWindow, videoWidth, videoHeight, cameraDeviceIndex, audioDeviceIndex)) {
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

  // Reset all participant info (except slot0 which is always local)
  slot1Info_->reset();
  slot2Info_->reset();
  slot3Info_->reset();

  emit connectedChanged();
  emit videoMutedChanged();
  emit audioMutedChanged();
}

bool AppController::isVideoMuted() const {
  return conference_ ? conference_->isVideoMuted() : false;
}

bool AppController::isAudioMuted() const {
  return conference_ ? conference_->isAudioMuted() : false;
}

void AppController::setVideoMuted(bool muted) {
  if (conference_ && conference_->setVideoMuted(muted)) {
    emit videoMutedChanged();
  }
}

void AppController::setAudioMuted(bool muted) {
  if (conference_ && conference_->setAudioMuted(muted)) {
    emit audioMutedChanged();
  }
}

