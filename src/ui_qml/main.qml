import QtQuick
import QtQuick.Controls

Window {
    id: root
    width: 920
    height: 620
    visible: true
    title: "IR Transceiver"
    color: "#0f172a"

    // Top docked header bar
    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
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

    // Main content below the top bar
    Column {
        anchors.top: topBar.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 18
        width: 680

        Text {
            text: "IR Transceiver"
            font.pixelSize: 32
            font.bold: true
            color: "#e2e8f0"
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

    Popup {
        id: capturePopup
        modal: true
        focus: true
        width: 320
        height: appController.captureReady ? 180 : 120
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        visible: appController.captureInProgress || appController.captureReady

        background: Rectangle {
            radius: 12
            color: "#111827"
            border.color: "#374151"
            border.width: 1
        }

        Column {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Row {
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter
                visible: appController.captureInProgress || appController.captureReady

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: appController.captureReady ? "#22c55e" : "#f59e0b"
                    border.color: "#f8fafc"
                    border.width: 1
                }

                Text {
                    text: appController.captureReady ? "Signal captured" : "Capturing..."
                    font.pixelSize: 17
                    font.bold: true
                    color: "#e5e7eb"
                }
            }

            BusyIndicator {
                running: appController.captureInProgress
                visible: appController.captureInProgress
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: appController.captureReady ? "Add a signal name and save it." : "Listening for IR signal..."
                color: "#cbd5e1"
                font.pixelSize: 13
                visible: true
            }

            TextField {
                width: 260
                visible: appController.captureReady
                text: appController.captureName
                onEditingFinished: appController.captureName = text
                placeholderText: "Signal name"
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Row {
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter

                Button {
                    visible: appController.captureReady
                    text: "Save"
                    onClicked: appController.saveCapturedSignal()
                }

                Button {
                    text: appController.captureReady ? "Discard" : "Cancel"
                    onClicked: appController.cancelCapture()
                }
            }
        }
    }
}
