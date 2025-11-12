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
    width: 640
    height: 480
    x: 30
    y: 30
    color: "black"

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: "Host:"; color: "white" }
            TextField { id: hostField; placeholderText: "meet.jit.si"; text: "t6s78uzq.ki.kormix.io"; selectByMouse: true; Layout.preferredWidth: 180 }
            Label { text: "Room:"; color: "white" }
            TextField { id: roomField; placeholderText: "myroom"; text: "video"; selectByMouse: true; Layout.preferredWidth: 140 }
            Label { text: "WxH:"; color: "white" }
            TextField { id: widthField; placeholderText: "1280"; text: "1280"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 70 }
            TextField { id: heightField; placeholderText: "720"; text: "720"; inputMethodHints: Qt.ImhDigitsOnly; Layout.preferredWidth: 60 }
            Button {
                id: connectButton
                text: AppController.connected ? "Connected" : "Connect"
                enabled: !AppController.connected
                onClicked: {
                    const w = parseInt(widthField.text) || 1280
                    const h = parseInt(heightField.text) || 720
                    AppController.connectToConference(window, hostField.text, roomField.text, w, h, 4, 720)
                }
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
        GstGLQt6VideoItem { objectName: "videoItem0"; Layout.fillWidth: true; Layout.fillHeight: true }
        GstGLQt6VideoItem { objectName: "videoItem1"; Layout.fillWidth: true; Layout.fillHeight: true }
        GstGLQt6VideoItem { objectName: "videoItem2"; Layout.fillWidth: true; Layout.fillHeight: true }
        GstGLQt6VideoItem { objectName: "videoItem3"; Layout.fillWidth: true; Layout.fillHeight: true }
    }
}
