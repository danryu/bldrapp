import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Dialogs 6.0
import QtQuick.Window 6.0

import org.freedesktop.gstreamer.Qt6GLVideoItem 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 640
    height: 480
    x: 30
    y: 30
    color: "black"

    Item {
        anchors.fill: parent

        GstGLQt6VideoItem {
            id: video
            objectName: "videoItem"
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
        }

        Rectangle {
            id: controlsBackground
            color: "#80000000" // Semi-transparent black
            height: 60
            width: parent.width
            anchors.top: parent.top
            
            ComboBox {
                id: cameraSelector
                anchors.centerIn: parent
                width: 300
                
                model: cameraManager.cameraNames
                currentIndex: cameraManager.currentCameraIndex
                
                onActivated: (index) => {
                    cameraManager.currentCameraIndex = index
                }
            }
        }
    }
}
