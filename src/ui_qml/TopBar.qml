import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: 56
    color: "#071127"
    border.color: "#0b1220"
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 18

        Text {
            text: "IR"
            font.pixelSize: 20
            font.bold: true
            color: "#e2e8f0"
            anchors.verticalCenter: parent.verticalCenter
        }

        // Server state (server process reachable)
        Row {
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: appController.serverRunning ? "#22c55e" : "#ef4444"
                border.color: appController.serverRunning ? "#86efac" : "#fca5a5"
                border.width: 1
            }

            Text {
                text: "Server: " + (appController.serverRunning ? "Running" : "Stopped")
                color: appController.serverRunning ? "#86efac" : "#fca5a5"
                font.pixelSize: 14
            }
        }

        // USB IR device state
        Row {
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: appController.irDeviceConnected ? "#22c55e" : (appController.deviceState === "Unknown" ? "#94a3b8" : "#ef4444")
                border.color: appController.irDeviceConnected ? "#86efac" : "#fca5a5"
                border.width: 1
            }

            Text {
                text: "USB IR: " + (appController.irDeviceConnected ? "Connected" : "Disconnected")
                color: appController.irDeviceConnected ? "#86efac" : "#fca5a5"
                font.pixelSize: 14
            }
        }

        // IP address
        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12

            Text {
                text: "IP:"
                color: "#94a3b8"
                font.pixelSize: 13
            }

            Text {
                text: appController.apiHost && appController.apiHost !== "" ? appController.apiHost : "-"
                color: "#cbd5e1"
                font.pixelSize: 13
                font.bold: true
            }
        }
    }
}
