#include "camera_manager.h"

#include <gst/gst.h>
#include <QDebug>
#include <algorithm>

CameraManager::CameraManager(QObject* parent)
    : QObject(parent) {
}

bool CameraManager::enumerateCameras() {
    cameras_.clear();

    // Use GstDeviceMonitor for proper device enumeration
    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if (!monitor) {
        qWarning() << "Failed to create device monitor";
        cameras_.emplace_back(nullptr, "Default Camera");
        currentCameraIndex_ = 0;
        emit currentCameraIndexChanged();
        emit camerasChanged();
        return true;
    }

    // Add filter for video sources
    GstCaps* caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(monitor, "Video/Source", caps);
    gst_caps_unref(caps);

    // Start monitoring
    if (!gst_device_monitor_start(monitor)) {
        qWarning() << "Failed to start device monitor";
        gst_object_unref(monitor);
        cameras_.emplace_back(nullptr, "Default Camera");
        currentCameraIndex_ = 0;
        emit currentCameraIndexChanged();
        emit camerasChanged();
        return true;
    }

    // Get devices
    GList* devices = gst_device_monitor_get_devices(monitor);

    for (GList* l = devices; l != nullptr; l = l->next) {
        GstDevice* device = GST_DEVICE(l->data);
        gchar* name = gst_device_get_display_name(device);
        GstElement* element = gst_device_create_element(device, nullptr);

        if (element) {
            // Check if this is an avfvideosrc element
            const gchar* factoryName = gst_plugin_feature_get_name(
                GST_PLUGIN_FEATURE(gst_element_get_factory(element)));

            if (g_strcmp0(factoryName, "avfvideosrc") == 0) {
                std::string displayName = name ? std::string(name) : "Unknown Camera";
                // Store the GstDevice directly - CameraDevice will ref it
                cameras_.emplace_back(device, displayName);
                qDebug() << "Found camera:" << QString::fromStdString(displayName);
            }

            gst_object_unref(element);
        }

        if (name) g_free(name);
        // Unref the device from the list - CameraDevice already took its own ref
        gst_object_unref(device);
    }

    g_list_free(devices);
    gst_device_monitor_stop(monitor);
    gst_object_unref(monitor);

    // If no cameras found, add default fallback (nullptr device)
    if (cameras_.empty()) {
        qWarning() << "No cameras found, adding default camera fallback";
        cameras_.emplace_back(nullptr, "Default Camera");
    }

    // Sort cameras by display name to ensure stable order across enumerations
    std::sort(cameras_.begin(), cameras_.end(), [](const CameraDevice& a, const CameraDevice& b) {
        return a.displayName < b.displayName;
    });

    // Select first camera by default
    if (!cameras_.empty() && currentCameraIndex_ < 0) {
        currentCameraIndex_ = 0;
        emit currentCameraIndexChanged();
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
