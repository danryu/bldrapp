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
  slot4Info_ = std::make_unique<ParticipantInfo>(this);
  slot5Info_ = std::make_unique<ParticipantInfo>(this);
  slot6Info_ = std::make_unique<ParticipantInfo>(this);
  slot7Info_ = std::make_unique<ParticipantInfo>(this);
  slot8Info_ = std::make_unique<ParticipantInfo>(this);
  slot9Info_ = std::make_unique<ParticipantInfo>(this);
  slot10Info_ = std::make_unique<ParticipantInfo>(this);
  slot11Info_ = std::make_unique<ParticipantInfo>(this);
  slot12Info_ = std::make_unique<ParticipantInfo>(this);
  slot13Info_ = std::make_unique<ParticipantInfo>(this);
  slot14Info_ = std::make_unique<ParticipantInfo>(this);
  slot15Info_ = std::make_unique<ParticipantInfo>(this);

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
  conference_->setParticipantInfoSlots(
      slot0Info_.get(), slot1Info_.get(), slot2Info_.get(), slot3Info_.get(),
      slot4Info_.get(), slot5Info_.get(), slot6Info_.get(), slot7Info_.get(),
      slot8Info_.get(), slot9Info_.get(), slot10Info_.get(), slot11Info_.get(),
      slot12Info_.get(), slot13Info_.get(), slot14Info_.get(), slot15Info_.get());

  // Re-enumerate cameras to get fresh GstDevice objects
  // This is necessary on macOS where AVFoundation device handles become stale after use
  QString selectedCameraName;
  if (cameraManager_) {
    const CameraDevice* currentSelection = cameraManager_->selectedCamera();
    if (currentSelection) {
      selectedCameraName = QString::fromStdString(currentSelection->displayName);
    }

    cameraManager_->enumerateCameras();

    // Re-select the same camera by name (device-index values are volatile)
    if (!selectedCameraName.isEmpty()) {
      for (int i = 0; i < cameraManager_->cameraNames().size(); ++i) {
        if (cameraManager_->cameraNames()[i] == selectedCameraName) {
          cameraManager_->setCurrentCameraIndex(i);
          break;
        }
      }
    }
  }

  // Get selected camera device (now fresh)
  GstDevice* cameraDevice = nullptr;
  if (cameraManager_) {
    const CameraDevice* selectedCamera = cameraManager_->selectedCamera();
    if (selectedCamera) {
      cameraDevice = selectedCamera->device;
      qDebug() << "Using camera:" << QString::fromStdString(selectedCamera->displayName);
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

  if (!conference_->initQmlSlotsAndSend(rootWindow, videoWidth, videoHeight, cameraDevice, audioDeviceIndex)) {
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

  // CRITICAL ORDERING for safe teardown:
  // 1. Disconnect signals first - prevents new callbacks from jitsibin
  conference_->disconnectSignals();

  // 2. Set pipeline to NULL - this stops jitsibin's internal tasks and
  //    waits for the runner thread to finish. After this returns,
  //    no more signals will be emitted.
  if (pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
  }

  // 3. Now safe to destroy Conference (ParticipantManager, SendPipeline)
  //    since no callbacks are possible
  conference_.reset();

  // 4. Finally unref the pipeline after Conference is gone
  if (pipeline) {
    gst_object_unref(pipeline);
  }

  // Reset all participant info (except slot0 which is always local)
  slot1Info_->reset();
  slot2Info_->reset();
  slot3Info_->reset();
  slot4Info_->reset();
  slot5Info_->reset();
  slot6Info_->reset();
  slot7Info_->reset();
  slot8Info_->reset();
  slot9Info_->reset();
  slot10Info_->reset();
  slot11Info_->reset();
  slot12Info_->reset();
  slot13Info_->reset();
  slot14Info_->reset();
  slot15Info_->reset();

  // Reset mute states for local participant
  slot0Info_->setVideoMuted(false);
  slot0Info_->setAudioMuted(false);

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
  if (muted) {
    // Muting: simple state change (to NULL)
    if (conference_ && conference_->setVideoMuted(true)) {
      slot0Info_->setVideoMuted(true);
      emit videoMutedChanged();
    }
  } else {
    // Unmuting: requires fresh GstDevice on macOS
    GstDevice* freshDevice = nullptr;
    
    if (cameraManager_) {
      // 1. Get current selection name
      QString selectedCameraName;
      const CameraDevice* currentSelection = cameraManager_->selectedCamera();
      if (currentSelection) {
        selectedCameraName = QString::fromStdString(currentSelection->displayName);
      }
      
      // 2. Re-enumerate to get fresh handles (fixes stale handle issue)
      cameraManager_->enumerateCameras();
      
      // 3. Find the camera again by name and get fresh device
      if (!selectedCameraName.isEmpty()) {
        for (int i = 0; i < cameraManager_->cameraNames().size(); ++i) {
          if (cameraManager_->cameraNames()[i] == selectedCameraName) {
            cameraManager_->setCurrentCameraIndex(i);
            const CameraDevice* newSelection = cameraManager_->selectedCamera();
            if (newSelection) {
              freshDevice = newSelection->device;
            }
            break;
          }
        }
      }
    }
    
    // 4. Unmute with fresh device
    if (conference_ && conference_->setVideoMuted(false, freshDevice)) {
      slot0Info_->setVideoMuted(false);
      emit videoMutedChanged();
    }
  }
}

void AppController::setAudioMuted(bool muted) {
  if (conference_ && conference_->setAudioMuted(muted)) {
    // Update local participant info to reflect mute state
    slot0Info_->setAudioMuted(muted);
    emit audioMutedChanged();
  }
}

