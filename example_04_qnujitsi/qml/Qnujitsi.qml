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
        ConferenceController.slot0Info,
        ConferenceController.slot1Info,
        ConferenceController.slot2Info,
        ConferenceController.slot3Info,
        ConferenceController.slot4Info,
        ConferenceController.slot5Info,
        ConferenceController.slot6Info,
        ConferenceController.slot7Info,
        ConferenceController.slot8Info,
        ConferenceController.slot9Info,
        ConferenceController.slot10Info,
        ConferenceController.slot11Info,
        ConferenceController.slot12Info,
        ConferenceController.slot13Info,
        ConferenceController.slot14Info,
        ConferenceController.slot15Info
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
            participantInfo: ConferenceController.slot0Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot0Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem1"
            participantInfo: ConferenceController.slot1Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot1Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem2"
            participantInfo: ConferenceController.slot2Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot2Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem3"
            participantInfo: ConferenceController.slot3Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot3Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem4"
            participantInfo: ConferenceController.slot4Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot4Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem5"
            participantInfo: ConferenceController.slot5Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot5Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem6"
            participantInfo: ConferenceController.slot6Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot6Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem7"
            participantInfo: ConferenceController.slot7Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot7Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem8"
            participantInfo: ConferenceController.slot8Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot8Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem9"
            participantInfo: ConferenceController.slot9Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot9Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem10"
            participantInfo: ConferenceController.slot10Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot10Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem11"
            participantInfo: ConferenceController.slot11Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot11Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem12"
            participantInfo: ConferenceController.slot12Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot12Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem13"
            participantInfo: ConferenceController.slot13Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot13Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem14"
            participantInfo: ConferenceController.slot14Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot14Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
        VideoSlot {
            videoObjectName: "videoItem15"
            participantInfo: ConferenceController.slot15Info
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ConferenceController.slot15Info.isActive
            Layout.preferredWidth: visible ? 100 : 0
            Layout.preferredHeight: visible ? 100 : 0
        }
    }
}
