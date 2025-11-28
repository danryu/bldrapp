import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import org.freedesktop.gstreamer.Qt6GLVideoItem 1.0

Item {
    id: root

    // Expose the video item's objectName for C++ lookup
    property alias videoObjectName: videoItem.objectName

    // Reference to ParticipantInfo object from C++
    property var participantInfo: null

    // The actual video item
    GstGLQt6VideoItem {
        id: videoItem
        anchors.fill: parent
    }

    // Overlay layer
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "transparent"
        visible: participantInfo && participantInfo.isActive

        // Top-left: Participant name
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 8
            width: nameLabel.width + 16
            height: nameLabel.height + 8
            color: "#CC000000"
            radius: 4
            visible: participantInfo && participantInfo.name !== ""

            Label {
                id: nameLabel
                anchors.centerIn: parent
                text: participantInfo ? participantInfo.name : ""
                color: "white"
                font.pixelSize: 14
                font.bold: true
            }
        }

        // Bottom-right: Status icons
        Row {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            spacing: 4

            Rectangle {
                width: 32
                height: 32
                color: "#CC000000"
                radius: 16
                visible: participantInfo && participantInfo.audioMuted

                Label {
                    anchors.centerIn: parent
                    text: "🎤"
                    font.pixelSize: 16
                    opacity: 0.5  // Dimmed when muted
                }
            }

            Rectangle {
                width: 32
                height: 32
                color: "#CC000000"
                radius: 16
                visible: participantInfo && participantInfo.videoMuted

                Label {
                    anchors.centerIn: parent
                    text: "📷"
                    font.pixelSize: 16
                    opacity: 0.5  // Dimmed when muted
                }
            }
        }
    }
}
