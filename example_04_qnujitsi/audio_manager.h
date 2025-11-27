// AudioManager: handles enumeration and management of audio capture devices
#pragma once

#include <QObject>
#include <QStringList>
#include <vector>
#include <string>

struct AudioDevice {
    std::string deviceName;    // GStreamer device name (e.g., "0" or device path)
    std::string displayName;   // Human-readable name

    AudioDevice(const std::string& devName, const std::string& dispName)
        : deviceName(devName), displayName(dispName) {}
};

class AudioManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList audioDeviceNames READ audioDeviceNames NOTIFY audioDevicesChanged)
    Q_PROPERTY(int currentAudioDeviceIndex READ currentAudioDeviceIndex WRITE setCurrentAudioDeviceIndex NOTIFY currentAudioDeviceIndexChanged)

public:
    explicit AudioManager(QObject* parent = nullptr);

    // Enumerate available audio input devices using GStreamer
    bool enumerateAudioDevices();

    // Get list of audio device display names for UI
    QStringList audioDeviceNames() const;

    // Get the selected audio device info
    const AudioDevice* selectedAudioDevice() const;

    // Get/set current audio device index
    int currentAudioDeviceIndex() const { return currentAudioDeviceIndex_; }
    void setCurrentAudioDeviceIndex(int index);

    // Get device by index
    const AudioDevice* audioDeviceAt(int index) const;

signals:
    void audioDevicesChanged();
    void currentAudioDeviceIndexChanged();

private:
    std::vector<AudioDevice> audioDevices_;
    int currentAudioDeviceIndex_ {-1};
};

