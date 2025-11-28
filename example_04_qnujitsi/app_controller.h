#pragma once

#include <QObject>
#include <memory>

#include "camera_manager.h"
#include "audio_manager.h"
#include "participant_info.h"

class QQuickWindow;
class Conference;

class AppController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
  Q_PROPERTY(CameraManager* cameraManager READ cameraManager CONSTANT)
  Q_PROPERTY(AudioManager* audioManager READ audioManager CONSTANT)
  Q_PROPERTY(bool videoMuted READ isVideoMuted WRITE setVideoMuted NOTIFY videoMutedChanged)
  Q_PROPERTY(bool audioMuted READ isAudioMuted WRITE setAudioMuted NOTIFY audioMutedChanged)
  Q_PROPERTY(ParticipantInfo* slot0Info READ slot0Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot1Info READ slot1Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot2Info READ slot2Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot3Info READ slot3Info CONSTANT)
public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  Q_INVOKABLE bool connectToConference(QQuickWindow* rootWindow,
                                       const QString& host,
                                       const QString& room,
                                       int videoWidth = 1280,
                                       int videoHeight = 720,
                                       int receiveLimit = 4,
                                       int receiveMaxHeight = 720);

  Q_INVOKABLE void disconnectConference();

  bool isConnected() const;
  CameraManager* cameraManager() const { return cameraManager_.get(); }
  AudioManager* audioManager() const { return audioManager_.get(); }

  // Participant info for each slot
  ParticipantInfo* slot0Info() const { return slot0Info_.get(); }
  ParticipantInfo* slot1Info() const { return slot1Info_.get(); }
  ParticipantInfo* slot2Info() const { return slot2Info_.get(); }
  ParticipantInfo* slot3Info() const { return slot3Info_.get(); }

  // Mute controls
  bool isVideoMuted() const;
  bool isAudioMuted() const;
  void setVideoMuted(bool muted);
  void setAudioMuted(bool muted);

signals:
  void connectedChanged();
  void videoMutedChanged();
  void audioMutedChanged();
  void error(const QString& message);

private:
  void teardown();

  std::unique_ptr<Conference> conference_;
  std::unique_ptr<CameraManager> cameraManager_;
  std::unique_ptr<AudioManager> audioManager_;

  // Participant info for each slot (owned by AppController, exposed to QML)
  std::unique_ptr<ParticipantInfo> slot0Info_;
  std::unique_ptr<ParticipantInfo> slot1Info_;
  std::unique_ptr<ParticipantInfo> slot2Info_;
  std::unique_ptr<ParticipantInfo> slot3Info_;
};


