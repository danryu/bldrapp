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
            Button {
                id: muteVideoButton
                text: AppController.videoMuted ? "📷 Off" : "📷 On"
                enabled: AppController.connected
                onClicked: AppController.videoMuted = !AppController.videoMuted
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
            Button {
                id: muteAudioButton
                text: AppController.audioMuted ? "🎤 Off" : "🎤 On"
                enabled: AppController.connected
                onClicked: AppController.audioMuted = !AppController.audioMuted
            }
            Label { text: "Host:"; color: "white" }
            TextField { id: hostField; placeholderText: "meet.jit.si"; text: "0yg8c4go.ki.kormix.io"; selectByMouse: true; Layout.preferredWidth: 160 }
            Label { text: "Room:"; color: "white" }
            TextField { id: roomField; placeholderText: "myroom"; text: "video"; selectByMouse: true; Layout.preferredWidth: 60 }
            Label { text: "WxH:"; color: "white" }
            // TextField { id: widthField; placeholderText: "1280"; text: "1280"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 60 }
            // TextField { id: heightField; placeholderText: "720"; text: "720"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 50 }
            Button {
                id: connectButton
                text: "Connect"
                enabled: !AppController.connected
                onClicked: {
                    AppController.connectToConference(window, hostField.text, roomField.text, 1280, 720, 15, 720)
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
        }
    }

    Item {
        id: videoContainer
        anchors.fill: parent
        anchors.margins: 8

        // Calculate active participants and optimal layout
        property var activeSlots: [
            AppController.slot0Info,
            AppController.slot1Info,
            AppController.slot2Info,
            AppController.slot3Info,
            AppController.slot4Info,
            AppController.slot5Info,
            AppController.slot6Info,
            AppController.slot7Info,
            AppController.slot8Info,
            AppController.slot9Info,
            AppController.slot10Info,
            AppController.slot11Info,
            AppController.slot12Info,
            AppController.slot13Info,
            AppController.slot14Info,
            AppController.slot15Info
        ]

        property int activeCount: {
            var count = 0
            for (var i = 0; i < activeSlots.length; i++) {
                if (activeSlots[i] && activeSlots[i].isActive) count++
            }
            return count
        }

        // 4x4 grid for up to 16 participants
        property int gridColumns: 4

        GridLayout {
            id: grid
            anchors.fill: parent
            columns: videoContainer.gridColumns
            rowSpacing: 8
            columnSpacing: 8

            // Pre-create 16 slots; C++ will bind sinks to these in order.
            VideoSlot {
                videoObjectName: "videoItem0"
                participantInfo: AppController.slot0Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot0Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem1"
                participantInfo: AppController.slot1Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot1Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem2"
                participantInfo: AppController.slot2Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot2Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem3"
                participantInfo: AppController.slot3Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot3Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem4"
                participantInfo: AppController.slot4Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot4Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem5"
                participantInfo: AppController.slot5Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot5Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem6"
                participantInfo: AppController.slot6Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot6Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem7"
                participantInfo: AppController.slot7Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot7Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem8"
                participantInfo: AppController.slot8Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot8Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem9"
                participantInfo: AppController.slot9Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot9Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem10"
                participantInfo: AppController.slot10Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot10Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem11"
                participantInfo: AppController.slot11Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot11Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem12"
                participantInfo: AppController.slot12Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot12Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem13"
                participantInfo: AppController.slot13Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot13Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem14"
                participantInfo: AppController.slot14Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot14Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
            VideoSlot {
                videoObjectName: "videoItem15"
                participantInfo: AppController.slot15Info
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AppController.slot15Info.isActive
                Layout.preferredWidth: visible ? 100 : 0
                Layout.preferredHeight: visible ? 100 : 0
            }
        }
    }
}
