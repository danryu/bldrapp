#pragma once

#include <QObject>
#include <QString>

class ParticipantInfo : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
  Q_PROPERTY(bool audioMuted READ audioMuted WRITE setAudioMuted NOTIFY audioMutedChanged)
  Q_PROPERTY(bool videoMuted READ videoMuted WRITE setVideoMuted NOTIFY videoMutedChanged)
  Q_PROPERTY(bool isActive READ isActive WRITE setIsActive NOTIFY isActiveChanged)
  Q_PROPERTY(QString participantId READ participantId WRITE setParticipantId NOTIFY participantIdChanged)

public:
  explicit ParticipantInfo(QObject* parent = nullptr);

  QString name() const { return name_; }
  void setName(const QString& name);

  bool audioMuted() const { return audioMuted_; }
  void setAudioMuted(bool muted);

  bool videoMuted() const { return videoMuted_; }
  void setVideoMuted(bool muted);

  bool isActive() const { return isActive_; }
  void setIsActive(bool active);

  QString participantId() const { return participantId_; }
  void setParticipantId(const QString& id);

  void reset();

signals:
  void nameChanged();
  void audioMutedChanged();
  void videoMutedChanged();
  void isActiveChanged();
  void participantIdChanged();

private:
  QString name_;
  QString participantId_;
  bool audioMuted_ = false;
  bool videoMuted_ = false;
  bool isActive_ = false;
};
