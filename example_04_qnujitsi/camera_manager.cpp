#include "camera_manager.h"

#include <gst/gst.h>
#include <QDebug>

CameraManager::CameraManager(QObject* parent)
    : QObject(parent) {
}

bool CameraManager::enumerateCameras() {
    cameras_.clear();

    // Use GstDeviceMonitor for proper device enumeration
    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if (!monitor) {
        qWarning() << "Failed to create device monitor";
        cameras_.emplace_back("0", "Default Camera");
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
        cameras_.emplace_back("0", "Default Camera");
        currentCameraIndex_ = 0;
        emit currentCameraIndexChanged();
        emit camerasChanged();
        return true;
    }

    // Get devices
    GList* devices = gst_device_monitor_get_devices(monitor);
    int deviceIndex = 0;

    for (GList* l = devices; l != nullptr; l = l->next) {
        GstDevice* device = GST_DEVICE(l->data);
        gchar* name = gst_device_get_display_name(device);
        GstElement* element = gst_device_create_element(device, nullptr);

        if (element) {
            // Check if this is an avfvideosrc element
            const gchar* factoryName = gst_plugin_feature_get_name(
                GST_PLUGIN_FEATURE(gst_element_get_factory(element)));

            if (g_strcmp0(factoryName, "avfvideosrc") == 0) {
                std::string displayName = name ? std::string(name) : ("Camera " + std::to_string(deviceIndex));
                cameras_.emplace_back(std::to_string(deviceIndex), displayName);
                qDebug() << "Found camera" << deviceIndex << ":" << QString::fromStdString(displayName);
                deviceIndex++;
            }

            gst_object_unref(element);
        }

        if (name) g_free(name);
        gst_object_unref(device);
    }

    g_list_free(devices);
    gst_device_monitor_stop(monitor);
    gst_object_unref(monitor);

    // If no cameras found via device monitor, fall back to simple enumeration
    if (cameras_.empty()) {
        qWarning() << "No cameras found via device monitor, trying fallback enumeration";

        // Try device indices 0-2 as fallback
        for (int i = 0; i < 3; ++i) {
            GstElement* testSrc = gst_element_factory_make("avfvideosrc", nullptr);
            if (!testSrc) break;

            g_object_set(G_OBJECT(testSrc), "device-index", i, NULL);
            GstStateChangeReturn ret = gst_element_set_state(testSrc, GST_STATE_READY);

            if (ret != GST_STATE_CHANGE_FAILURE) {
                gchar* deviceName = nullptr;
                g_object_get(G_OBJECT(testSrc), "device-name", &deviceName, NULL);

                std::string displayName;
                if (deviceName && strlen(deviceName) > 0) {
                    displayName = std::string(deviceName);
                    g_free(deviceName);
                } else {
                    displayName = "Camera " + std::to_string(i);
                }

                cameras_.emplace_back(std::to_string(i), displayName);
                qDebug() << "Found camera (fallback)" << i << ":" << QString::fromStdString(displayName);
            }

            gst_element_set_state(testSrc, GST_STATE_NULL);
            gst_object_unref(testSrc);

            if (i == 0 && ret == GST_STATE_CHANGE_FAILURE) break;
            if (ret == GST_STATE_CHANGE_FAILURE) break;
        }
    }

    // Final fallback: add default camera
    if (cameras_.empty()) {
        qWarning() << "No cameras found, adding default camera 0";
        cameras_.emplace_back("0", "Default Camera");
    }

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
