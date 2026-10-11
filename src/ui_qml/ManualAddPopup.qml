import QtQuick
import QtQuick.Controls

Popup {
    id: manualAddPopup
    modal: true
    focus: true
    width: 420
    height: 320
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#0b1220"; radius: 8; border.color: "#1f2937" }

    property string selectedProtocol: "Raw"

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text { text: "Add Signal Manually"; color: "#e2e8f0"; font.pixelSize: 16; font.bold: true }

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
            TextField { id: manualFreq; placeholderText: "Frequency (kHz)"; text: "38" }
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
                        for (var i=0;i<parts.length;i++) {
                            var v = parseInt(parts[i]);
                            if (!isNaN(v)) pulsesArr.push(v);
                        }
                    }
                    var proto = manualAddPopup.selectedProtocol || "Raw";
                    appController.addSignalManually(manualName.text, proto, freq, raw, pulsesArr);
                    manualAddPopup.close();
                }
            }

            Button { text: "Cancel"; onClicked: manualAddPopup.close() }
        }
    }
}
