import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: triggerPopup
    modal: true
    focus: true
    width: 620
    height: 480
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: parent

    background: Rectangle {
        radius: 12
        color: "#0f172a"
        border.color: appController.triggerModeActive ? "#38bdf8" : "#334155"
        border.width: appController.triggerModeActive ? 2 : 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 14
                height: 14
                radius: 7
                color: appController.triggerModeActive ? "#22c55e" : "#64748b"
                border.color: appController.triggerModeActive ? "#86efac" : "#94a3b8"
                border.width: 1
            }

            Text {
                text: "Trigger Mode"
                font.pixelSize: 18
                font.bold: true
                color: "#f8fafc"
            }

            Rectangle {
                visible: appController.triggerModeActive
                height: 20
                radius: 4
                color: "#0c4a6e"
                border.color: "#38bdf8"
                border.width: 1
                implicitWidth: activeLabel.implicitWidth + 10
                Layout.alignment: Qt.AlignVCenter

                Text {
                    id: activeLabel
                    anchors.centerIn: parent
                    text: "ACTIVE"
                    font.pixelSize: 10
                    font.bold: true
                    color: "#38bdf8"
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "✕"
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                font.pixelSize: 18
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: "Close dialog"
                onClicked: triggerPopup.close()
            }
        }

        // Status banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            radius: 6
            color: appController.triggerModeActive ? "#042f2e" : "#1e293b"
            border.color: appController.triggerModeActive ? "#14b8a6" : "#334155"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text {
                    text: appController.triggerStatusText
                    color: appController.triggerModeActive ? "#5eead4" : "#94a3b8"
                    font.pixelSize: 12
                    font.bold: appController.triggerModeActive
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        // Two columns: Left (Armed Triggers), Right (Activity Log)
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // Left panel: Armed Triggers
            Rectangle {
                Layout.preferredWidth: 270
                Layout.fillHeight: true
                radius: 8
                color: "#071127"
                border.color: "#1e293b"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Armed Triggers (" + appController.armedTriggerCount + ")"
                            color: "#e2e8f0"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    // Warning if no armed signals
                    Rectangle {
                        visible: appController.armedTriggerCount === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "transparent"

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 20
                            wrapMode: Text.WordWrap
                            text: "No signals with shell commands found.\n\nUse Edit (✎) on a signal in the main list to configure an automated shell command."
                            color: "#94a3b8"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    ListView {
                        id: armedList
                        visible: appController.armedTriggerCount > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: appController.armedSignals

                        delegate: Rectangle {
                            width: armedList.width
                            height: 54
                            radius: 6
                            color: "#0f172a"
                            border.color: "#1e293b"
                            border.width: 1

                            Column {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 2

                                Row {
                                    spacing: 6
                                    width: parent.width

                                    Rectangle {
                                        height: 14
                                        radius: 3
                                        color: "#1e293b"
                                        border.color: "#38bdf8"
                                        border.width: 1
                                        implicitWidth: devBadge.implicitWidth + 6
                                        anchors.verticalCenter: parent.verticalCenter

                                        Text {
                                            id: devBadge
                                            anchors.centerIn: parent
                                            text: modelData.rcdName || "Device"
                                            color: "#38bdf8"
                                            font.pixelSize: 9
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        text: modelData.name || "Signal"
                                        color: "#f8fafc"
                                        font.pixelSize: 12
                                        font.bold: true
                                        elide: Text.ElideRight
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                Text {
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

            // Right panel: Activity Log
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: "#071127"
                border.color: "#1e293b"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Activity Log"
                            color: "#e2e8f0"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "Clear"
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: 64
                            font.pixelSize: 12
                            ToolTip.visible: hovered
                            ToolTip.delay: 500
                            ToolTip.text: "Clear activity log"
                            onClicked: appController.clearTriggerLog()
                        }
                    }

                    ListView {
                        id: logList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: appController.triggerLog

                        delegate: Rectangle {
                            width: logList.width
                            height: logText.implicitHeight + 6
                            color: modelData.indexOf("TRIGGER") !== -1 ? "#0c4a6e" : (modelData.indexOf("WARNING") !== -1 || modelData.indexOf("ERROR") !== -1 ? "#3b1115" : "#0f172a")
                            radius: 4
                            border.color: modelData.indexOf("TRIGGER") !== -1 ? "#38bdf8" : "#1e293b"
                            border.width: 1

                            Text {
                                id: logText
                                anchors.fill: parent
                                anchors.margins: 4
                                text: modelData
                                color: modelData.indexOf("TRIGGER") !== -1 ? "#e0f2fe" : (modelData.indexOf("WARNING") !== -1 || modelData.indexOf("ERROR") !== -1 ? "#fca5a5" : "#cbd5e1")
                                font.pixelSize: 11
                                font.family: "monospace"
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

        // Action Buttons Footer
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                id: triggerActionBtn
                Layout.preferredHeight: 44
                Layout.preferredWidth: 195
                text: appController.triggerModeActive ? "⏹ Stop Trigger Mode" : "▶ Start Trigger Mode"
                font.bold: true
                font.pixelSize: 13
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: appController.triggerModeActive ? "Stop listening for IR triggers" : "Start listening for IR triggers"
                contentItem: Text {
                    text: triggerActionBtn.text
                    font: triggerActionBtn.font
                    color: appController.triggerModeActive ? "#fca5a5" : "#86efac"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: appController.triggerModeActive ? "#450a0a" : "#052e16"
                    border.color: appController.triggerModeActive ? "#ef4444" : "#22c55e"
                    border.width: 1
                }
                onClicked: appController.toggleTriggerMode()
            }

            Item { Layout.fillWidth: true }

            Button {
                Layout.preferredHeight: 44
                Layout.preferredWidth: 160
                text: appController.triggerModeActive ? "Run in Background" : "Close"
                font.pixelSize: 13
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: appController.triggerModeActive ? "Keep trigger mode running in background" : "Close dialog"
                onClicked: triggerPopup.close()
            }
        }
    }
}
