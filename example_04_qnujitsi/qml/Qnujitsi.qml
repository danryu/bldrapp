// Qnujitsi.qml - Reusable Jitsi video conference component
// This component displays a 4x4 grid of video participants
// To use in your app: import this file and the Qnujitsi module

import QtQuick 6.0
import QtQuick.Layouts 6.0
import Qnujitsi 1.0

Item {
    id: root

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
        columns: root.gridColumns
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
