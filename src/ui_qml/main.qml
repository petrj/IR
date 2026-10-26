import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
    TopBar { id: topBar; serverStatusDialog: serverStatusDialog; startServerDialog: serverStatusDialog; usbStatusDialog: usbStatusDialog; triggerPopup: triggerPopup }

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
                    id: headerRow
                    spacing: 10
                    Text {
                        text: "Saved Signals"
                        color: "#e2e8f0"
                        font.pixelSize: 16
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Button {
                        text: "↻"
                        width: 42
                        height: 38
                        font.pixelSize: 18
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Refresh signals list"
                        onClicked: appController.refreshSignals()
                    }
                    Button {
                        text: "+"
                        width: 42
                        height: 38
                        font.pixelSize: 22
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Add signal manually"
                        onClicked: manualAddPopup.openForNew()
                    }
                    Button {
                        text: "📡"
                        width: 42
                        height: 38
                        font.pixelSize: 18
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Capture IR signal from transceiver"
                        onClicked: appController.startCapture()
                    }

                    Button {
                        id: triggerModeBtn
                        text: appController.triggerModeActive ? "⚡ Trigger Active" : "⚡ Trigger Mode"
                        height: 38
                        font.pixelSize: 13
                        font.bold: true
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Configure trigger mode and automated actions"
                        contentItem: Text {
                            text: triggerModeBtn.text
                            font: triggerModeBtn.font
                            color: appController.triggerModeActive ? "#38bdf8" : "#cbd5e1"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                            rightPadding: 10
                        }
                        background: Rectangle {
                            radius: 6
                            color: appController.triggerModeActive ? "#0c4a6e" : "#1e293b"
                            border.color: appController.triggerModeActive ? "#38bdf8" : "#475569"
                            border.width: 1
                        }
                        onClicked: triggerPopup.open()
                    }

                    ComboBox {
                        id: deviceFilterCombo
                        width: 180
                        height: 38
                        font.pixelSize: 13
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
                    height: parent.height - headerRow.height - 18
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
                        height: (modelData.shellCommand && modelData.shellCommand.length > 0) ? 92 : 76
                        radius: 8
                        color: index % 2 === 0 ? "#071a2a" : "#061423"
                        border.color: "#1e293b"
                        border.width: 1

                        Row {
                            id: actionButtonsRow
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10

                            Button {
                                text: "✎"
                                width: 44
                                height: 42
                                font.pixelSize: 18
                                ToolTip.visible: hovered
                                ToolTip.delay: 500
                                ToolTip.text: "View signal details / edit"
                                onClicked: manualAddPopup.openForEdit(modelData.index)
                            }
                            Button {
                                text: "➤"
                                width: 48
                                height: 42
                                font.pixelSize: 20
                                ToolTip.visible: hovered
                                ToolTip.delay: 500
                                ToolTip.text: "Transmit IR signal"
                                onClicked: appController.transmitSavedSignal(modelData.index)
                            }
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: actionButtonsRow.left
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: manualAddPopup.openForEdit(modelData.index)
                            }

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

                            Text {
                                visible: !!modelData.shellCommand && modelData.shellCommand.length > 0
                                text: "cmd: " + (modelData.shellCommand || "")
                                color: "#38bdf8"
                                font.pixelSize: 10
                                width: parent.width
                                elide: Text.ElideRight
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
        width: 360
        standardButtons: Dialog.Ok | Dialog.Cancel

        function openForSignal(idx, name) {
            root.currentDeleteIndex = idx;
            root.currentDeleteName = name || "Unnamed";
            open();
        }

        contentItem: Rectangle {
            color: "#0f172a"
            implicitWidth: 320
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
        id: serverStatusDialog
        title: "Server Status"
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

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Flickable {
                    id: serverFlickable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentHeight: serverStatusText.implicitHeight + 8
                    contentWidth: width

                    Text {
                        id: serverStatusText
                        width: serverFlickable.width
                        wrapMode: Text.WordWrap
                        text: appController.serverStatusDetails
                        color: "#e2e8f0"
                        font.pixelSize: 13
                        lineHeight: 1.5
                    }
                }

                Button {
                    id: serverActionBtn
                    visible: !appController.serverRunning
                    Layout.alignment: Qt.AlignHCenter
                    text: "Start Server"
                    implicitHeight: 42
                    implicitWidth: 140
                    font.pixelSize: 14
                    font.bold: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Start local IR API server"
                    onClicked: {
                        appController.startServer();
                    }
                }
            }
        }
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
    ManualAddPopup { id: manualAddPopup; deleteDialog: deleteSignalDialog }

    CapturePopup { id: capturePopup }
 
    TriggerPopup { id: triggerPopup }
}
