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
  Q_PROPERTY(ParticipantInfo* slot4Info READ slot4Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot5Info READ slot5Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot6Info READ slot6Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot7Info READ slot7Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot8Info READ slot8Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot9Info READ slot9Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot10Info READ slot10Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot11Info READ slot11Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot12Info READ slot12Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot13Info READ slot13Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot14Info READ slot14Info CONSTANT)
  Q_PROPERTY(ParticipantInfo* slot15Info READ slot15Info CONSTANT)
public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  Q_INVOKABLE bool connectToConference(QQuickWindow* rootWindow,
                                       const QString& host,
                                       const QString& room,
                                       int videoWidth = 1280,
                                       int videoHeight = 720,
                                       int receiveLimit = 15,
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
  ParticipantInfo* slot4Info() const { return slot4Info_.get(); }
  ParticipantInfo* slot5Info() const { return slot5Info_.get(); }
  ParticipantInfo* slot6Info() const { return slot6Info_.get(); }
  ParticipantInfo* slot7Info() const { return slot7Info_.get(); }
  ParticipantInfo* slot8Info() const { return slot8Info_.get(); }
  ParticipantInfo* slot9Info() const { return slot9Info_.get(); }
  ParticipantInfo* slot10Info() const { return slot10Info_.get(); }
  ParticipantInfo* slot11Info() const { return slot11Info_.get(); }
  ParticipantInfo* slot12Info() const { return slot12Info_.get(); }
  ParticipantInfo* slot13Info() const { return slot13Info_.get(); }
  ParticipantInfo* slot14Info() const { return slot14Info_.get(); }
  ParticipantInfo* slot15Info() const { return slot15Info_.get(); }

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
  std::unique_ptr<ParticipantInfo> slot4Info_;
  std::unique_ptr<ParticipantInfo> slot5Info_;
  std::unique_ptr<ParticipantInfo> slot6Info_;
  std::unique_ptr<ParticipantInfo> slot7Info_;
  std::unique_ptr<ParticipantInfo> slot8Info_;
  std::unique_ptr<ParticipantInfo> slot9Info_;
  std::unique_ptr<ParticipantInfo> slot10Info_;
  std::unique_ptr<ParticipantInfo> slot11Info_;
  std::unique_ptr<ParticipantInfo> slot12Info_;
  std::unique_ptr<ParticipantInfo> slot13Info_;
  std::unique_ptr<ParticipantInfo> slot14Info_;
  std::unique_ptr<ParticipantInfo> slot15Info_;
};


