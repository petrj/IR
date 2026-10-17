import QtQuick
import QtQuick.Controls

Popup {
    id: manualAddPopup
    modal: true
    focus: true
    width: 440
    height: 380
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    background: Rectangle { color: "#0b1220"; radius: 8; border.color: "#1f2937" }

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

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Text {
            text: manualAddPopup.editing ? "Edit Signal" : "Add Signal Manually"
            color: "#e2e8f0"
            font.pixelSize: 16
            font.bold: true
        }

        TextField { id: manualName; placeholderText: "Signal Name"; width: parent.width }

        Row {
            width: parent.width
            spacing: 8

            ComboBox {
                id: manualRcdCombo
                width: manualAddPopup.isNewRcdSelected ? 180 : parent.width
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
                placeholderText: "New Remote Device name"
            }
        }

        Row {
            spacing: 8
            Button {
                text: "Raw"
                checkable: true
                checked: manualAddPopup.selectedProtocol === "Raw"
                onClicked: manualAddPopup.selectedProtocol = "Raw"
            }
            Button {
                text: "NEC"
                checkable: true
                checked: manualAddPopup.selectedProtocol === "NEC"
                onClicked: manualAddPopup.selectedProtocol = "NEC"
            }
            Row {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter

                TextField {
                    id: manualFreq
                    placeholderText: "Frequency"
                    text: "38"
                    width: 100
                }

                Text {
                    text: "kHz"
                    color: "#cbd5e1"
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        TextArea { id: manualRawHex; placeholderText: "rawHex (optional)"; height: 36; width: parent.width }
        TextArea { id: manualPulses; placeholderText: "Pulses (comma-separated microseconds)"; height: 64; width: parent.width }

        Row {
            spacing: 12
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 4

            Button {
                text: "Save"
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

                    if (manualAddPopup.editing) {
                        appController.updateSignal(manualAddPopup.editingIndex, manualName.text, proto, freq, raw, pulsesArr, chosenRcd);
                    } else {
                        appController.addSignalManually(manualName.text, proto, freq, raw, pulsesArr, chosenRcd);
                    }
                    manualAddPopup.close();
                }
            }

            Button { text: "Cancel"; onClicked: manualAddPopup.close() }
        }
    }
}
