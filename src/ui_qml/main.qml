import QtQuick
import QtQuick.Controls

Window {
    id: root
    width: 920
    height: 620
    visible: true
    title: "IR Transceiver"
    color: "#0f172a"

    // Top docked header bar (moved to TopBar.qml)
    TopBar { id: topBar }

    // Main content below the top bar
    Column {
        anchors.top: topBar.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 18
        width: 760

        // Saved signals grid
        Rectangle {
            width: 760
            height: 360
            color: "#071127"
            radius: 10
            border.color: "#0b1220"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Row {
                    spacing: 8
                    Text { text: "Saved Signals"; color: "#e2e8f0"; font.pixelSize: 16; font.bold: true }
                    Button { text: "Refresh"; onClicked: appController.refreshSignals() }
                    Button { text: "Add Manual"; onClicked: manualAddPopup.open() }
                    Button { text: "Capture via API"; onClicked: appController.startCapture() }
                }

                GridView {
                    id: signalGrid
                    model: appController.savedSignals
                    width: parent.width
                    height: parent.height - 40
                    clip: true
                    cellWidth: 220
                    cellHeight: 130
                    delegate: Rectangle {
                        width: signalGrid.cellWidth - 12
                        height: signalGrid.cellHeight - 12
                        radius: 8
                        color: index % 2 === 0 ? "#071a2a" : "#061423"
                        border.color: "#1e293b"
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Text {
                                text: modelData.name || "Unnamed"
                                color: "#f8fafc"
                                font.pixelSize: 15
                                font.bold: true
                                width: parent.width
                                elide: Text.ElideRight
                            }

                            Text {
                                text: (modelData.protocol || "Raw") + " • " + (modelData.pulseCount || 0) + " pulses"
                                color: "#cbd5e1"
                                font.pixelSize: 12
                            }

                            Text {
                                text: "Freq: " + (modelData.frequency || 38) + " kHz"
                                color: "#94a3b8"
                                font.pixelSize: 11
                            }

                            Text {
                                text: modelData.timestamp || ""
                                color: "#64748b"
                                font.pixelSize: 11
                                width: parent.width
                                elide: Text.ElideRight
                            }

                            Row {
                                spacing: 8
                                anchors.horizontalCenter: parent.horizontalCenter
                                Button {
                                    text: "Send"
                                    onClicked: appController.transmitSavedSignal(modelData.index)
                                }
                                Button {
                                    text: "Delete"
                                    onClicked: appController.removeSavedSignal(modelData.index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Floating capture button (oval) bottom-right
    Button {
        id: captureButton
        text: "Capture"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 96
        height: 48
        font.pixelSize: 16
        background: Rectangle {
            color: "#10b981"
            radius: height / 2
            border.color: "#059669"
            border.width: 1
        }
        contentItem: Text { text: captureButton.text; color: "white"; anchors.centerIn: parent }
        onClicked: appController.startCapture()
    }

    // Manual add popup (moved to ManualAddPopup.qml)
    ManualAddPopup { id: manualAddPopup }

    CapturePopup { id: capturePopup }
}
