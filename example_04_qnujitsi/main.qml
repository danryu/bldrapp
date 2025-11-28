import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Dialogs 6.0
import QtQuick.Window 6.0
import QtQuick.Layouts 6.0

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

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: "Cam:"; color: "white" }
            ComboBox {
                id: cameraCombo
                model: AppController.cameraManager.cameraNames
                currentIndex: AppController.cameraManager.currentCameraIndex
                enabled: !AppController.connected
                onCurrentIndexChanged: {
                    if (currentIndex !== AppController.cameraManager.currentCameraIndex) {
                        AppController.cameraManager.currentCameraIndex = currentIndex
                    }
                }
                Layout.preferredWidth: 140
            }
            Label { text: "Mic:"; color: "white" }
            ComboBox {
                id: audioCombo
                model: AppController.audioManager.audioDeviceNames
                currentIndex: AppController.audioManager.currentAudioDeviceIndex
                enabled: !AppController.connected
                onCurrentIndexChanged: {
                    if (currentIndex !== AppController.audioManager.currentAudioDeviceIndex) {
                        AppController.audioManager.currentAudioDeviceIndex = currentIndex
                    }
                }
                Layout.preferredWidth: 140
            }
            Label { text: "Host:"; color: "white" }
            TextField { id: hostField; placeholderText: "meet.jit.si"; text: "chht6m4h.ki.kormix.io"; selectByMouse: true; Layout.preferredWidth: 160 }
            Label { text: "Room:"; color: "white" }
            TextField { id: roomField; placeholderText: "myroom"; text: "video"; selectByMouse: true; Layout.preferredWidth: 100 }
            Label { text: "WxH:"; color: "white" }
            TextField { id: widthField; placeholderText: "1280"; text: "1280"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 60 }
            TextField { id: heightField; placeholderText: "720"; text: "720"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 50 }
            Button {
                id: connectButton
                text: "Connect"
                enabled: !AppController.connected
                onClicked: {
                    const w = parseInt(widthField.text) || 1280
                    const h = parseInt(heightField.text) || 720
                    AppController.connectToConference(window, hostField.text, roomField.text, w, h, 4, 720)
                }
            }
            Button {
                id: disconnectButton
                text: "Disconnect"
                enabled: AppController.connected
                onClicked: {
                    AppController.disconnectConference()
                }
            }
            Button {
                id: muteVideoButton
                text: AppController.videoMuted ? "📷 Off" : "📷 On"
                enabled: AppController.connected
                onClicked: AppController.videoMuted = !AppController.videoMuted
            }
            Button {
                id: muteAudioButton
                text: AppController.audioMuted ? "🎤 Off" : "🎤 On"
                enabled: AppController.connected
                onClicked: AppController.audioMuted = !AppController.audioMuted
            }
        }
    }

    GridLayout {
        id: grid
        anchors.fill: parent
        columns: 2
        rowSpacing: 8
        columnSpacing: 8
        anchors.margins: 8

        // Pre-create a few slots; C++ will bind sinks to these in order.
        VideoSlot {
            videoObjectName: "videoItem0"
            participantInfo: AppController.slot0Info
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        VideoSlot {
            videoObjectName: "videoItem1"
            participantInfo: AppController.slot1Info
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        VideoSlot {
            videoObjectName: "videoItem2"
            participantInfo: AppController.slot2Info
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        VideoSlot {
            videoObjectName: "videoItem3"
            participantInfo: AppController.slot3Info
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
