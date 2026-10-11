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
        width: 680

        // Saved signals list
        Rectangle {
            width: 680
            color: "#071127"
            radius: 8
            border.color: "#0b1220"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Row {
                    spacing: 8
                    Text { text: "Saved Signals"; color: "#e2e8f0"; font.pixelSize: 16; font.bold: true }
                    Button { text: "Refresh"; onClicked: appController.refreshSignals() }
                    Button { text: "Add"; onClicked: manualAddPopup.open() }
                }

                ListView {
                    model: appController.savedSignals
                    height: 200
                    clip: true
                    delegate: Rectangle {
                        width: parent.width
                        height: 34
                        color: index % 2 === 0 ? "#071a2a" : "#061423"
                        Row {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 12
                            Text { text: name; color: "#cbd5e1" }
                            Text { text: protocol + " (" + pulseCount + ")"; color: "#94a3b8" }
                            Text { text: timestamp; color: "#64748b" }
                        }
                    }
                    Component.onCompleted: {
                        // initial population happens via binding to appController.savedSignals
                    }
                }
            }
        }

        // floating Capture button is placed at window level (see below)
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
