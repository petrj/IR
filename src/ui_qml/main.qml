import QtQuick
import QtQuick.Controls

Window {
    id: root
    width: 920
    height: 620
    visible: true
    title: "IR Transceiver"
    color: "#0f172a"

    property int currentDeleteIndex: -1
    property string currentDeleteName: ""

    // Top docked header bar (moved to TopBar.qml)
    TopBar { id: topBar; startServerDialog: startServerDialog; usbStatusDialog: usbStatusDialog }

    // Main content below the top bar
    Column {
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        spacing: 12

        // Saved signals list
        Rectangle {
            width: parent.width
            height: parent.height - 16
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
                    Text {
                        text: "Saved Signals"
                        color: "#e2e8f0"
                        font.pixelSize: 16
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Button {
                        text: "↻"
                        width: 32
                        height: 28
                        font.pixelSize: 14
                        onClicked: appController.refreshSignals()
                    }
                    Button {
                        text: "+"
                        width: 32
                        height: 28
                        font.pixelSize: 18
                        onClicked: manualAddPopup.openForNew()
                    }
                    Button {
                        text: "📡"
                        width: 32
                        height: 28
                        font.pixelSize: 14
                        onClicked: appController.startCapture()
                    }

                    ComboBox {
                        id: deviceFilterCombo
                        width: 170
                        height: 28
                        font.pixelSize: 12
                        model: {
                            var m = ["All Devices"];
                            for (var i = 0; i < appController.rcdList.length; ++i) {
                                m.push(appController.rcdList[i]);
                            }
                            return m;
                        }
                        onCurrentTextChanged: {
                            appController.selectedDeviceFilter = currentText;
                        }
                    }
                }

                ListView {
                    id: signalList
                    model: appController.savedSignals
                    width: parent.width
                    height: parent.height - 40
                    clip: true
                    spacing: 8
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick
                    ScrollBar.vertical: ScrollBar {
                        id: listScrollBar
                        active: true
                        visible: signalList.contentHeight > signalList.height
                        policy: ScrollBar.AlwaysOn
                        width: 8
                        contentItem: Rectangle {
                            radius: width / 2
                            color: "#94a3b8"
                            opacity: 0.9
                        }
                        background: Rectangle {
                            radius: width / 2
                            color: "#1e293b"
                            opacity: 0.6
                        }
                    }
                    delegate: Rectangle {
                        width: signalList.width - 10
                        height: 68
                        radius: 8
                        color: index % 2 === 0 ? "#071a2a" : "#061423"
                        border.color: "#1e293b"
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 12

                            Column {
                                width: parent.width - 120
                                spacing: 3

                                Row {
                                    spacing: 8
                                    width: parent.width

                                    Rectangle {
                                        height: 18
                                        radius: 4
                                        color: "#1e293b"
                                        border.color: "#38bdf8"
                                        border.width: 1
                                        implicitWidth: rcdBadgeText.implicitWidth + 8
                                        anchors.verticalCenter: parent.verticalCenter

                                        Text {
                                            id: rcdBadgeText
                                            anchors.centerIn: parent
                                            text: modelData.rcdName || "Default Remote Device"
                                            color: "#38bdf8"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        text: modelData.name || "Unnamed"
                                        color: "#f8fafc"
                                        font.pixelSize: 14
                                        font.bold: true
                                        elide: Text.ElideRight
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                Text {
                                    text: (modelData.protocol || "Raw") + " • " + (modelData.pulseCount || 0) + " pulses • " + (modelData.frequency || 38) + " kHz"
                                    color: "#cbd5e1"
                                    font.pixelSize: 11
                                }

                                Text {
                                    text: modelData.timestamp || ""
                                    color: "#64748b"
                                    font.pixelSize: 10
                                    width: parent.width
                                    elide: Text.ElideRight
                                }
                            }

                            Row {
                                spacing: 8
                                anchors.verticalCenter: parent.verticalCenter
                                Button {
                                    text: "✎"
                                    width: 30
                                    height: 26
                                    font.pixelSize: 12
                                    onClicked: manualAddPopup.openForEdit(modelData.index)
                                }
                                Button {
                                    text: "➤"
                                    width: 30
                                    height: 26
                                    font.pixelSize: 12
                                    onClicked: appController.transmitSavedSignal(modelData.index)
                                }
                                Button {
                                    text: "🗑"
                                    width: 30
                                    height: 26
                                    font.pixelSize: 12
                                    onClicked: {
                                        root.currentDeleteIndex = modelData.index;
                                        root.currentDeleteName = modelData.name || "Unnamed";
                                        deleteSignalDialog.open();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteSignalDialog
        title: "Delete saved signal?"
        modal: true
        anchors.centerIn: parent
        width: 340
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Rectangle {
            color: "#0f172a"
            implicitWidth: 300
            implicitHeight: 90
            radius: 8
            border.color: "#1e293b"
            border.width: 1

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                wrapMode: Text.WordWrap
                text: "Delete \"" + root.currentDeleteName + "\" from your saved signals?"
                color: "#e2e8f0"
                font.pixelSize: 14
            }
        }

        onAccepted: appController.removeSavedSignal(root.currentDeleteIndex)
    }

    Dialog {
        id: startServerDialog
        title: "Start API server?"
        modal: true
        anchors.centerIn: parent
        width: 340
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Rectangle {
            color: "#0f172a"
            implicitWidth: 300
            implicitHeight: 90
            radius: 8
            border.color: "#1e293b"
            border.width: 1

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                wrapMode: Text.WordWrap
                text: "Start the local IR API server?"
                color: "#e2e8f0"
                font.pixelSize: 14
            }
        }

        onAccepted: appController.startServer()
    }

    Dialog {
        id: usbStatusDialog
        title: "USB IR Status"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Close

        contentItem: Rectangle {
            color: "#0f172a"
            implicitWidth: 380
            implicitHeight: 180
            radius: 10
            border.color: "#1e293b"
            border.width: 1

            Flickable {
                anchors.fill: parent
                anchors.margins: 16
                clip: true
                contentHeight: statusText.implicitHeight + 8
                contentWidth: width

                Text {
                    id: statusText
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: appController.usbStatusDetails
                    color: "#e2e8f0"
                    font.pixelSize: 13
                    lineHeight: 1.5
                }
            }
        }
    }

    // Manual add popup (moved to ManualAddPopup.qml)
    ManualAddPopup { id: manualAddPopup }

    CapturePopup { id: capturePopup }
}
