import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Dialogs 6.0
import QtQuick.Window 6.0
import QtQuick.Layouts 6.0

import org.freedesktop.gstreamer.Qt6GLVideoItem 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    x: 30
    y: 30
    color: "black"

    RowLayout {
        id: controls
        spacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8

        TextField {
            id: hostField
            Layout.fillWidth: true
            placeholderText: "Enter Jitsi host (e.g. meet.jit.si)"
            text: ""
        }
        Button {
            text: "Connect"
            onClicked: {
                controller.setRoot(window);
                controller.connectTo(hostField.text);
            }
        }
        Button {
            text: "Disconnect"
            onClicked: controller.disconnect()
        }
    }

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: controls.bottom
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
