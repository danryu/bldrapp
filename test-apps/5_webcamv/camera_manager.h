// CameraManager: handles enumeration and management of video capture devices
#pragma once

#include <QObject>
#include <QStringList>
#include <vector>
#include <string>
#include <gst/gst.h>

// Forward declare GstDevice
typedef struct _GstDevice GstDevice;

struct CameraDevice {
    GstDevice* device;         // GstDevice object (ref-counted)
    std::string displayName;   // Human-readable name

    CameraDevice(GstDevice* dev, const std::string& dispName)
        : device(dev), displayName(dispName) {
        if (device) gst_object_ref(device);
    }

    ~CameraDevice() {
        if (device) gst_object_unref(device);
    }

    // Disable copy, enable move
    CameraDevice(const CameraDevice&) = delete;
    CameraDevice& operator=(const CameraDevice&) = delete;
    CameraDevice(CameraDevice&& other) noexcept
        : device(other.device), displayName(std::move(other.displayName)) {
        other.device = nullptr;
    }
    CameraDevice& operator=(CameraDevice&& other) noexcept {
        if (this != &other) {
            if (device) gst_object_unref(device);
            device = other.device;
            displayName = std::move(other.displayName);
            other.device = nullptr;
        }
        return *this;
    }
};

class CameraManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList cameraNames READ cameraNames NOTIFY camerasChanged)
    Q_PROPERTY(int currentCameraIndex READ currentCameraIndex WRITE setCurrentCameraIndex NOTIFY currentCameraIndexChanged)

public:
    explicit CameraManager(QObject* parent = nullptr);
    ~CameraManager();

    // Enumerate available camera devices using GStreamer
    bool enumerateCameras();

    // Get list of camera display names for UI
    QStringList cameraNames() const;

    // Get/set current camera index
    int currentCameraIndex() const { return currentCameraIndex_; }
    void setCurrentCameraIndex(int index);

    // Create a GStreamer source element for the currently selected camera
    // Returns a floating reference (caller must take ownership or add to bin)
    GstElement* createCurrentCameraElement();

signals:
    void camerasChanged();
    void currentCameraIndexChanged();
    void cameraDeviceSelectionChanged(); 

private:
    std::vector<CameraDevice> cameras_;
    int currentCameraIndex_ {-1};
};
