// CameraManager: handles enumeration and management of video capture devices
#pragma once

#include <QObject>
#include <QStringList>
#include <vector>
#include <string>

struct CameraDevice {
    std::string deviceName;    // GStreamer device name (e.g., "0" or device path)
    std::string displayName;   // Human-readable name

    CameraDevice(const std::string& devName, const std::string& dispName)
        : deviceName(devName), displayName(dispName) {}
};

class CameraManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList cameraNames READ cameraNames NOTIFY camerasChanged)
    Q_PROPERTY(int currentCameraIndex READ currentCameraIndex WRITE setCurrentCameraIndex NOTIFY currentCameraIndexChanged)

public:
    explicit CameraManager(QObject* parent = nullptr);

    // Enumerate available camera devices using GStreamer
    bool enumerateCameras();

    // Get list of camera display names for UI
    QStringList cameraNames() const;

    // Get the selected camera device info
    const CameraDevice* selectedCamera() const;

    // Get/set current camera index
    int currentCameraIndex() const { return currentCameraIndex_; }
    void setCurrentCameraIndex(int index);

    // Get device by index
    const CameraDevice* cameraAt(int index) const;

signals:
    void camerasChanged();
    void currentCameraIndexChanged();

private:
    std::vector<CameraDevice> cameras_;
    int currentCameraIndex_ {-1};
};
