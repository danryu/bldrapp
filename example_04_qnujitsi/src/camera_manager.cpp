#include "camera_manager.h"
#include "platform_elements.h"

#include <gst/gst.h>
#include <QDebug>
#include <algorithm>

CameraManager::CameraManager(QObject* parent)
    : QObject(parent) {
}

CameraManager::~CameraManager() {
    cameras_.clear();
}

bool CameraManager::enumerateCameras() {
    // 1. Save current selection name to restore it after enumeration
    std::string currentName;
    if (currentCameraIndex_ >= 0 && currentCameraIndex_ < static_cast<int>(cameras_.size())) {
        currentName = cameras_[currentCameraIndex_].displayName;
    }

    cameras_.clear();

    // Use GstDeviceMonitor for proper device enumeration
    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if (!monitor) {
        qWarning() << "Failed to create device monitor";
        emit camerasChanged();
        return false;
    }

    // Add filter for video sources
    GstCaps* caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(monitor, "Video/Source", caps);
    gst_caps_unref(caps);

    // Start monitoring
    if (!gst_device_monitor_start(monitor)) {
        qWarning() << "Failed to start device monitor";
        gst_object_unref(monitor);
        emit camerasChanged();
        return false;
    }

    // Get devices
    GList* devices = gst_device_monitor_get_devices(monitor);

    for (GList* l = devices; l != nullptr; l = l->next) {
        GstDevice* device = GST_DEVICE(l->data);
        gchar* name = gst_device_get_display_name(device);
        
        // Trust the monitor filter "Video/Source" for basic filtering.
        // Platform specific checks could go here if needed, but are generally redundant
        // with the GstDeviceMonitor logic if configured correctly.

        std::string displayName = name ? std::string(name) : "Unknown Camera";
        
        // Store the GstDevice directly
        cameras_.emplace_back(device, displayName);
        qDebug() << "Found camera:" << QString::fromStdString(displayName);
        
        if (name) g_free(name);
    }
    
    // Unref devices in the list (since we took our own refs in CameraDevice)
    g_list_free_full(devices, gst_object_unref);

    gst_device_monitor_stop(monitor);
    gst_object_unref(monitor);

    // Sort cameras by display name to ensure stable order
    std::sort(cameras_.begin(), cameras_.end(), [](const CameraDevice& a, const CameraDevice& b) {
        return a.displayName < b.displayName;
    });

    // 2. Restore selection by name
    int newIndex = -1;
    if (!currentName.empty()) {
        for (size_t i = 0; i < cameras_.size(); ++i) {
            if (cameras_[i].displayName == currentName) {
                newIndex = static_cast<int>(i);
                break;
            }
        }
    }

    // 3. Update index
    if (newIndex >= 0) {
        // Found the previous camera
        if (currentCameraIndex_ != newIndex) {
            currentCameraIndex_ = newIndex;
            emit currentCameraIndexChanged();
        }
        // Note: We do NOT emit cameraDeviceSelectionChanged here because the logical selection hasn't changed
    } else {
        // Previous camera not found, select first available or -1
        int nextIndex = cameras_.empty() ? -1 : 0;
        
        if (currentCameraIndex_ != nextIndex) {
            currentCameraIndex_ = nextIndex;
            emit currentCameraIndexChanged();
            emit cameraDeviceSelectionChanged(); // Selection implicitly changed
        }
    }

    emit camerasChanged();
    return !cameras_.empty();
}

QStringList CameraManager::cameraNames() const {
    QStringList names;
    for (const auto& camera : cameras_) {
        names << QString::fromStdString(camera.displayName);
    }
    return names;
}

const CameraDevice* CameraManager::selectedCamera() const {
    if (currentCameraIndex_ >= 0 && currentCameraIndex_ < static_cast<int>(cameras_.size())) {
        return &cameras_[currentCameraIndex_];
    }
    return nullptr;
}

void CameraManager::setCurrentCameraIndex(int index) {
    if (index >= 0 && index < static_cast<int>(cameras_.size()) && index != currentCameraIndex_) {
        currentCameraIndex_ = index;
        emit currentCameraIndexChanged();
        emit cameraDeviceSelectionChanged();
        qDebug() << "Selected camera" << index << ":"
                 << QString::fromStdString(cameras_[index].displayName);
    }
}

const CameraDevice* CameraManager::cameraAt(int index) const {
    if (index >= 0 && index < static_cast<int>(cameras_.size())) {
        return &cameras_[index];
    }
    return nullptr;
}
