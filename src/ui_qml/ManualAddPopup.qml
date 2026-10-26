import QtQuick
import QtQuick.Controls

Popup {
    id: manualAddPopup
    modal: true
    focus: true
    width: Math.min(460, parent ? (parent.width - 24) : 460)
    height: Math.min(520, parent ? (parent.height - 24) : 520)
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    background: Rectangle { color: "#0b1220"; radius: 8; border.color: "#1f2937" }

    property Dialog deleteDialog: null
    property bool editing: false
    property int editingIndex: -1
    property string selectedProtocol: "Raw"
    property bool isNewRcdSelected: (manualRcdCombo.currentText === "+ New Remote Device...") || (appController.rcdList.length === 0)

    function resetForm() {
        manualName.text = "";
        manualFreq.text = "38";
        manualRawHex.text = "";
        manualPulses.text = "";
        manualNewRcd.text = "";
        manualShellCommand.text = "";
        manualAddPopup.selectedProtocol = "Raw";
        editing = false;
        editingIndex = -1;
        if (manualRcdCombo.count > 0) {
            manualRcdCombo.currentIndex = 0;
        }
    }

    function openForNew() {
        resetForm();
        open();
    }

    function openForEdit(index) {
        editing = true;
        editingIndex = index;

        var signal = appController.getSignalDetails(index);
        manualName.text = signal.name || "";
        manualFreq.text = signal.frequency || "38";
        manualRawHex.text = signal.rawHex || "";
        manualShellCommand.text = signal.shellCommand || "";
        manualAddPopup.selectedProtocol = signal.protocol || "Raw";

        var rcdName = signal.rcdName || "Default Remote Device";
        var foundIdx = -1;
        for (var i = 0; i < manualRcdCombo.count; ++i) {
            if (manualRcdCombo.textAt(i) === rcdName) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx >= 0) {
            manualRcdCombo.currentIndex = foundIdx;
        } else {
            manualNewRcd.text = rcdName;
            manualRcdCombo.currentIndex = manualRcdCombo.count - 1; // "+ New Remote Device..."
        }

        var pulses = signal.pulses || [];
        var pulseText = "";
        for (var p = 0; p < pulses.length; ++p) {
            if (p > 0) pulseText += ",";
            pulseText += pulses[p];
        }
        manualPulses.text = pulseText;

        open();
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: width
        contentHeight: formColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: formColumn
            width: parent.width
            spacing: 10

            Text {
                text: manualAddPopup.editing ? "Signal Details" : "Add Signal Manually"
                color: "#e2e8f0"
                font.pixelSize: 18
                font.bold: true
            }

            TextField {
                id: manualName
                placeholderText: "Signal Name"
                width: parent.width
                implicitHeight: 40
                font.pixelSize: 14
            }

            Row {
                width: parent.width
                spacing: 8

                ComboBox {
                    id: manualRcdCombo
                    width: manualAddPopup.isNewRcdSelected ? 180 : parent.width
                    implicitHeight: 40
                    font.pixelSize: 13
                    model: {
                        var list = [];
                        for (var i = 0; i < appController.rcdList.length; ++i) {
                            list.push(appController.rcdList[i]);
                        }
                        list.push("+ New Remote Device...");
                        return list;
                    }
                }

                TextField {
                    id: manualNewRcd
                    visible: manualAddPopup.isNewRcdSelected
                    width: parent.width - manualRcdCombo.width - 8
                    implicitHeight: 40
                    font.pixelSize: 14
                    placeholderText: "New Remote Device name"
                }
            }

            Row {
                spacing: 10
                Button {
                    text: "Raw"
                    checkable: true
                    checked: manualAddPopup.selectedProtocol === "Raw"
                    implicitHeight: 38
                    implicitWidth: 80
                    font.pixelSize: 13
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Select Raw pulse timing format"
                    onClicked: manualAddPopup.selectedProtocol = "Raw"
                }
                Button {
                    text: "NEC"
                    checkable: true
                    checked: manualAddPopup.selectedProtocol === "NEC"
                    implicitHeight: 38
                    implicitWidth: 80
                    font.pixelSize: 13
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Select NEC IR protocol"
                    onClicked: manualAddPopup.selectedProtocol = "NEC"
                }
                Row {
                    spacing: 6
                    anchors.verticalCenter: parent.verticalCenter

                    TextField {
                        id: manualFreq
                        placeholderText: "Frequency"
                        text: "38"
                        width: 90
                        implicitHeight: 38
                        font.pixelSize: 13
                    }

                    Text {
                        text: "kHz"
                        color: "#cbd5e1"
                        font.pixelSize: 13
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            TextArea {
                id: manualRawHex
                placeholderText: "rawHex (optional)"
                implicitHeight: 40
                width: parent.width
                font.pixelSize: 13
            }
            TextArea {
                id: manualPulses
                placeholderText: "Pulses (comma-separated microseconds)"
                implicitHeight: 56
                width: parent.width
                font.pixelSize: 13
            }
            TextField {
                id: manualShellCommand
                placeholderText: "Shell Command (optional, e.g. amixer set Master 5%+)"
                implicitHeight: 40
                width: parent.width
                font.pixelSize: 13
            }

            Row {
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 6

                Button {
                    id: deleteSignalBtn
                    visible: manualAddPopup.editing
                    text: "🗑 Delete"
                    implicitHeight: 42
                    implicitWidth: 105
                    font.pixelSize: 13
                    font.bold: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Delete this signal"
                    contentItem: Text {
                        text: deleteSignalBtn.text
                        font: deleteSignalBtn.font
                        color: "#fca5a5"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: "#450a0a"
                        border.color: "#ef4444"
                        border.width: 1
                    }
                    onClicked: {
                        var idx = manualAddPopup.editingIndex;
                        var name = manualName.text;
                        manualAddPopup.close();
                        if (manualAddPopup.deleteDialog) {
                            manualAddPopup.deleteDialog.openForSignal(idx, name);
                        }
                    }
                }

                Button {
                    text: "Cancel"
                    implicitHeight: 42
                    implicitWidth: 95
                    font.pixelSize: 13
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Cancel and close"
                    onClicked: manualAddPopup.close()
                }

                Button {
                    text: "Save"
                    implicitHeight: 42
                    implicitWidth: 95
                    font.pixelSize: 13
                    font.bold: true
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Save IR signal"
                    onClicked: {
                        var freq = parseInt(manualFreq.text) || 38;
                        var raw = manualRawHex.text.trim();
                        var pulsesArr = [];
                        if (manualPulses.text.trim().length > 0) {
                            var parts = manualPulses.text.split(',');
                            for (var i = 0; i < parts.length; ++i) {
                                var v = parseInt(parts[i]);
                                if (!isNaN(v)) pulsesArr.push(v);
                            }
                        }
                        var proto = manualAddPopup.selectedProtocol || "Raw";

                        var chosenRcd = "";
                        if (!manualAddPopup.isNewRcdSelected && manualRcdCombo.currentText.length > 0) {
                            chosenRcd = manualRcdCombo.currentText;
                        } else {
                            chosenRcd = manualNewRcd.text.trim();
                            if (chosenRcd.length === 0) {
                                chosenRcd = "Default Remote Device";
                            }
                        }

                        var shellCmd = manualShellCommand.text.trim();

                        if (manualAddPopup.editing) {
                            appController.updateSignal(manualAddPopup.editingIndex, manualName.text, proto, freq, raw, pulsesArr, chosenRcd, shellCmd);
                        } else {
                            appController.addSignalManually(manualName.text, proto, freq, raw, pulsesArr, chosenRcd, shellCmd);
                        }
                        manualAddPopup.close();
                    }
                }
            }
        }
    }
}
