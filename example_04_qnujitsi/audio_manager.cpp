#include "audio_manager.h"

#include <gst/gst.h>
#include <QDebug>

AudioManager::AudioManager(QObject* parent)
    : QObject(parent) {
}

bool AudioManager::enumerateAudioDevices() {
    audioDevices_.clear();

    // Use GstDeviceMonitor for proper device enumeration
    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if (!monitor) {
        qWarning() << "Failed to create device monitor for audio";
        audioDevices_.emplace_back("0", "Default Microphone");
        currentAudioDeviceIndex_ = 0;
        emit currentAudioDeviceIndexChanged();
        emit audioDevicesChanged();
        return true;
    }

    // Add filter for audio sources
    GstCaps* caps = gst_caps_new_empty_simple("audio/x-raw");
    gst_device_monitor_add_filter(monitor, "Audio/Source", caps);
    gst_caps_unref(caps);

    // Start monitoring
    if (!gst_device_monitor_start(monitor)) {
        qWarning() << "Failed to start device monitor for audio";
        gst_object_unref(monitor);
        audioDevices_.emplace_back("0", "Default Microphone");
        currentAudioDeviceIndex_ = 0;
        emit currentAudioDeviceIndexChanged();
        emit audioDevicesChanged();
        return true;
    }

    // Get devices
    GList* devices = gst_device_monitor_get_devices(monitor);

    for (GList* l = devices; l != nullptr; l = l->next) {
        GstDevice* device = GST_DEVICE(l->data);
        gchar* name = gst_device_get_display_name(device);
        GstElement* element = gst_device_create_element(device, nullptr);

        if (element) {
            // Check if this is an osxaudiosrc element (macOS audio source)
            const gchar* factoryName = gst_plugin_feature_get_name(
                GST_PLUGIN_FEATURE(gst_element_get_factory(element)));

            if (g_strcmp0(factoryName, "osxaudiosrc") == 0) {
                // Get the actual CoreAudio device ID from the element
                // osxaudiosrc's "device" property is the AudioDeviceID, not a sequential index
                gint coreAudioDeviceId = 0;
                g_object_get(G_OBJECT(element), "device", &coreAudioDeviceId, NULL);

                std::string displayName = name ? std::string(name) : ("Microphone " + std::to_string(coreAudioDeviceId));
                audioDevices_.emplace_back(std::to_string(coreAudioDeviceId), displayName);
                qDebug() << "Found audio device with CoreAudio ID" << coreAudioDeviceId 
                         << ":" << QString::fromStdString(displayName);
            }

            gst_object_unref(element);
        }

        if (name) g_free(name);
        gst_object_unref(device);
    }

    g_list_free(devices);
    gst_device_monitor_stop(monitor);
    gst_object_unref(monitor);

    // If no audio devices found via device monitor, add system default option
    // Note: We can't enumerate CoreAudio devices by sequential index since device IDs are arbitrary
    if (audioDevices_.empty()) {
        qWarning() << "No audio devices found via device monitor, adding system default";
        // Empty string means "use system default" (no device property set on osxaudiosrc)
        audioDevices_.emplace_back("", "System Default Microphone");
    }

    // Select first audio device by default
    if (!audioDevices_.empty() && currentAudioDeviceIndex_ < 0) {
        currentAudioDeviceIndex_ = 0;
        emit currentAudioDeviceIndexChanged();
    }

    emit audioDevicesChanged();
    return !audioDevices_.empty();
}

QStringList AudioManager::audioDeviceNames() const {
    QStringList names;
    for (const auto& device : audioDevices_) {
        names << QString::fromStdString(device.displayName);
    }
    return names;
}

const AudioDevice* AudioManager::selectedAudioDevice() const {
    if (currentAudioDeviceIndex_ >= 0 && currentAudioDeviceIndex_ < static_cast<int>(audioDevices_.size())) {
        return &audioDevices_[currentAudioDeviceIndex_];
    }
    return nullptr;
}

void AudioManager::setCurrentAudioDeviceIndex(int index) {
    if (index >= 0 && index < static_cast<int>(audioDevices_.size()) && index != currentAudioDeviceIndex_) {
        currentAudioDeviceIndex_ = index;
        emit currentAudioDeviceIndexChanged();
        qDebug() << "Selected audio device" << index << ":"
                 << QString::fromStdString(audioDevices_[index].displayName);
    }
}

const AudioDevice* AudioManager::audioDeviceAt(int index) const {
    if (index >= 0 && index < static_cast<int>(audioDevices_.size())) {
        return &audioDevices_[index];
    }
    return nullptr;
}

