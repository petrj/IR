import QtQuick
import QtQuick.Controls

Popup {
    id: manualAddPopup
    modal: true
    focus: true
    width: 420
    height: 320
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    background: Rectangle { color: "#0b1220"; radius: 8; border.color: "#1f2937" }

    property bool editing: false
    property int editingIndex: -1
    property string selectedProtocol: "Raw"

    function resetForm() {
        manualName.text = "";
        manualFreq.text = "38";
        manualRawHex.text = "";
        manualPulses.text = "";
        manualAddPopup.selectedProtocol = "Raw";
        editing = false;
        editingIndex = -1;
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

        var pulses = signal.pulses || [];
        var pulseText = "";
        for (var i = 0; i < pulses.length; ++i) {
            if (i > 0) pulseText += ",";
            pulseText += pulses[i];
        }
        manualPulses.text = pulseText;

        open();
    }

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            text: manualAddPopup.editing ? "Edit Signal" : "Add Signal Manually"
            color: "#e2e8f0"
            font.pixelSize: 16
            font.bold: true
        }

        TextField { id: manualName; placeholderText: "Name"; width: parent.width }
        Row { spacing: 8
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
                    width: 120
                }

                Text {
                    text: "kHz"
                    color: "#cbd5e1"
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
        TextArea { id: manualRawHex; placeholderText: "rawHex (optional)"; height: 40; width: parent.width }
        TextArea { id: manualPulses; placeholderText: "Pulses (comma-separated microseconds)"; height: 80; width: parent.width }

        Row { spacing: 8; anchors.horizontalCenter: parent.horizontalCenter
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

                    if (manualAddPopup.editing) {
                        appController.updateSignal(manualAddPopup.editingIndex, manualName.text, proto, freq, raw, pulsesArr);
                    } else {
                        appController.addSignalManually(manualName.text, proto, freq, raw, pulsesArr);
                    }
                    manualAddPopup.close();
                }
            }

            Button { text: "Cancel"; onClicked: manualAddPopup.close() }
        }
    }
}
