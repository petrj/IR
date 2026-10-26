import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: 56
    color: "#071127"
    border.color: "#0b1220"
    border.width: 1

    property Dialog serverStatusDialog: null
    property Dialog startServerDialog: null
    property Dialog usbStatusDialog: null
    property Popup triggerPopup: null

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 18

        Text {
            text: "IR"
            font.pixelSize: 20
            font.bold: true
            color: "#e2e8f0"
        }

        // Server state (server process reachable)
        Item {
            implicitWidth: serverStatusRow.implicitWidth
            implicitHeight: 40
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: serverStatusRow
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

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    appController.refreshServerStatusDialog();
                    if (root.serverStatusDialog) {
                        root.serverStatusDialog.open();
                    } else if (root.startServerDialog) {
                        root.startServerDialog.open();
                    }
                }
            }
        }

        // Trigger Mode active badge
        Item {
            implicitWidth: triggerStatusRow.implicitWidth
            implicitHeight: 40
            Layout.alignment: Qt.AlignVCenter
            visible: appController.triggerModeActive

            Row {
                id: triggerStatusRow
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: "#38bdf8"
                    border.color: "#e0f2fe"
                    border.width: 1
                }

                Text {
                    text: "Trigger Active (" + appController.armedTriggerCount + ")"
                    color: "#38bdf8"
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.triggerPopup) root.triggerPopup.open()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 12
        }

        // USB IR device state
        Item {
            implicitWidth: usbStatusRow.implicitWidth
            implicitHeight: 40
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: usbStatusRow
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

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    appController.refreshUsbStatusDialog();
                    if (root.usbStatusDialog) {
                        root.usbStatusDialog.open();
                    }
                }
            }
        }
    }
}
