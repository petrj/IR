import QtQuick
import QtQuick.Controls

Popup {
    id: capturePopup
    modal: true
    focus: true
    width: 360
    height: appController.captureReady ? (isNewRcdSelected ? 360 : 310) : 120
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: parent
    visible: appController.captureInProgress || appController.captureReady

    property bool isNewRcdSelected: (rcdCombo.currentText === "+ Create New Remote Device...") || (appController.rcdList.length === 0)

    background: Rectangle {
        radius: 12
        color: "#111827"
        border.color: "#374151"
        border.width: 1
    }

    Column {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10

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
                text: appController.captureReady ? "Signal Captured" : "Capturing..."
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
            text: appController.captureReady ? "Specify Remote Control Device (RCD) and signal name:" : "Listening for IR signal..."
            color: "#cbd5e1"
            font.pixelSize: 12
            visible: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        TextField {
            id: signalNameField
            width: parent.width
            visible: appController.captureReady
            text: appController.captureName
            onEditingFinished: appController.captureName = text
            placeholderText: "Signal name (e.g. TV Power)"
        }

        Column {
            width: parent.width
            spacing: 6
            visible: appController.captureReady

            Text {
                text: "Remote Control Device (RCD):"
                color: "#94a3b8"
                font.pixelSize: 11
                font.bold: true
            }

            ComboBox {
                id: rcdCombo
                width: parent.width
                visible: appController.rcdList.length > 0
                model: {
                    var list = [];
                    for (var i = 0; i < appController.rcdList.length; ++i) {
                        list.push(appController.rcdList[i]);
                    }
                    list.push("+ Create New Remote Device...");
                    return list;
                }
            }

            TextField {
                id: newRcdField
                width: parent.width
                visible: capturePopup.isNewRcdSelected
                placeholderText: "Enter new Remote Device name"
                text: appController.rcdList.length === 0 ? "Default Remote Device" : ""
            }
        }

        TextField {
            id: captureShellCmdField
            width: parent.width
            visible: appController.captureReady
            placeholderText: "Shell Command (optional)"
        }

        Row {
            spacing: 12
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 4

            Button {
                visible: appController.captureReady
                text: "Save Signal"
                onClicked: {
                    var rcd = "";
                    if (!capturePopup.isNewRcdSelected && rcdCombo.currentText.length > 0) {
                        rcd = rcdCombo.currentText;
                    } else {
                        rcd = newRcdField.text.trim();
                        if (rcd.length === 0) {
                            rcd = "Default Remote Device";
                        }
                    }
                    var sName = signalNameField.text.trim();
                    if (sName.length === 0) {
                        sName = "Captured Signal";
                    }
                    appController.saveCapturedSignal(rcd, sName, captureShellCmdField.text.trim());
                }
            }

            Button {
                text: appController.captureReady ? "Discard" : "Cancel"
                onClicked: appController.cancelCapture()
            }
        }
    }
}
