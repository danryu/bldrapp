#include "participant_info.h"

ParticipantInfo::ParticipantInfo(QObject* parent)
    : QObject(parent) {
}

void ParticipantInfo::setName(const QString& name) {
  if (name_ != name) {
    name_ = name;
    emit nameChanged();
  }
}

void ParticipantInfo::setAudioMuted(bool muted) {
  if (audioMuted_ != muted) {
    audioMuted_ = muted;
    emit audioMutedChanged();
  }
}

void ParticipantInfo::setVideoMuted(bool muted) {
  if (videoMuted_ != muted) {
    videoMuted_ = muted;
    emit videoMutedChanged();
  }
}

void ParticipantInfo::setIsActive(bool active) {
  if (isActive_ != active) {
    isActive_ = active;
    emit isActiveChanged();
  }
}

void ParticipantInfo::setParticipantId(const QString& id) {
  if (participantId_ != id) {
    participantId_ = id;
    emit participantIdChanged();
  }
}

void ParticipantInfo::reset() {
  setName("");
  setParticipantId("");
  setAudioMuted(false);
  setVideoMuted(false);
  setIsActive(false);
}
