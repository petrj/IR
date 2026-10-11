import QtQuick
import QtQuick.Controls

Window {
    id: root
    width: 920
    height: 620
    visible: true
    title: "IR Transceiver"
    color: "#0f172a"

    Rectangle {
        anchors.fill: parent
        color: "#0f172a"

        Column {
            anchors.centerIn: parent
            spacing: 18
            width: 680

            Text {
                text: "IR Transceiver"
                font.pixelSize: 32
                font.bold: true
                color: "#e2e8f0"
            }

            Row {
                spacing: 12
                anchors.left: parent.left

                Rectangle {
                    width: 18
                    height: 18
                    radius: 9
                    color: appController.serverConnected ? "#22c55e" : "#ef4444"
                    border.color: appController.serverConnected ? "#86efac" : "#fca5a5"
                    border.width: 2
                }

                Text {
                    text: appController.serverConnected ? "Connected" : "Disconnected"
                    font.pixelSize: 18
                    color: appController.serverConnected ? "#86efac" : "#fca5a5"
                    font.bold: true
                }
            }

            TextField {
                width: 680
                text: appController.apiUrl
                onEditingFinished: appController.apiUrl = text
                placeholderText: "API URL"
            }

            Row {
                spacing: 16

                Button {
                    text: "Capture"
                    onClicked: appController.startCapture()
                }

                Button {
                    text: "Transmit"
                    onClicked: appController.transmitSignal()
                }
            }
        }
    }
}
