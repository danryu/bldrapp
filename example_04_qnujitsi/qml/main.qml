// Minimal Qt/QML application wrapper for Qnujitsi video client
// This serves as a reference for integrating Qnujitsi into your own Qt/QML applications

import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import QtQuick.Window 6.0

import org.freedesktop.gstreamer.Qt6GLVideoItem 1.0
import Qnujitsi 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 1140
    height: 480
    x: 30
    y: 30
    color: "black"

    // Simple toolbar with connection controls
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: "Cam:"; color: "white" }
            ComboBox {
                id: cameraCombo
                model: ConferenceController.cameraManager.cameraNames
                currentIndex: ConferenceController.cameraManager.currentCameraIndex
                enabled: !ConferenceController.connected
                onCurrentIndexChanged: {
                    if (currentIndex !== ConferenceController.cameraManager.currentCameraIndex) {
                        ConferenceController.cameraManager.currentCameraIndex = currentIndex
                    }
                }
                Layout.preferredWidth: 140
            }
            Button {
                id: muteVideoButton
                text: ConferenceController.videoMuted ? "📷 Off" : "📷 On"
                enabled: ConferenceController.connected
                onClicked: ConferenceController.videoMuted = !ConferenceController.videoMuted
            }
            Label { text: "Mic:"; color: "white" }
            ComboBox {
                id: audioCombo
                model: ConferenceController.audioManager.audioDeviceNames
                currentIndex: ConferenceController.audioManager.currentAudioDeviceIndex
                enabled: !ConferenceController.connected
                onCurrentIndexChanged: {
                    if (currentIndex !== ConferenceController.audioManager.currentAudioDeviceIndex) {
                        ConferenceController.audioManager.currentAudioDeviceIndex = currentIndex
                    }
                }
                Layout.preferredWidth: 140
            }
            Button {
                id: muteAudioButton
                text: ConferenceController.audioMuted ? "🎤 Off" : "🎤 On"
                enabled: ConferenceController.connected
                onClicked: ConferenceController.audioMuted = !ConferenceController.audioMuted
            }
            Label { text: "Host:"; color: "white" }
            TextField { id: hostField; placeholderText: "meet.jit.si"; text: "0yg8c4go.ki.kormix.io"; selectByMouse: true; Layout.preferredWidth: 160 }
            Label { text: "Room:"; color: "white" }
            TextField { id: roomField; placeholderText: "myroom"; text: "video"; selectByMouse: true; Layout.preferredWidth: 60 }
            Label { text: "WxH:"; color: "white" }
            Button {
                id: connectButton
                text: "Connect"
                enabled: !ConferenceController.connected
                onClicked: {
                    ConferenceController.connectToConference(window, hostField.text, roomField.text, 1280, 720, 15, 720)
                }
            }
            Button {
                id: disconnectButton
                text: "Disconnect"
                enabled: ConferenceController.connected
                onClicked: {
                    ConferenceController.disconnectConference()
                }
            }
        }
    }

    // The Qnujitsi video client component - this is what you'd integrate into your app
    Item {
        id: videoContainer
        anchors.fill: parent
        anchors.margins: 8

        Qnujitsi {
            anchors.fill: parent
        }
    }
}
