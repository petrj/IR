import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string iconType: "" // "refresh", "add", "capture", "trigger", "edit", "transmit", "close", "trash"
    property bool active: false
    property color activeColor: "#38bdf8"
    property color activeBgColor: "#0c4a6e"
    property color activeBorderColor: "#38bdf8"
    property color normalColor: "#cbd5e1"
    property color hoverColor: "#ffffff"
    property color pressedColor: "#94a3b8"
    property color normalBgColor: "#1e293b"
    property color hoverBgColor: "#24324a"
    property color pressedBgColor: "#334155"
    property color normalBorderColor: "#475569"
    property color hoverBorderColor: "#64748b"
    property string tooltipText: ""
    property int iconSize: 20
    property int buttonWidth: 42
    property int buttonHeight: 38

    implicitWidth: buttonWidth
    implicitHeight: buttonHeight
    width: buttonWidth
    height: buttonHeight

    ToolTip.visible: hovered && tooltipText.length > 0
    ToolTip.delay: 500
    ToolTip.text: tooltipText

    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    background: Rectangle {
        implicitWidth: control.buttonWidth
        implicitHeight: control.buttonHeight
        radius: 6
        color: {
            if (control.active) {
                return control.down ? Qt.darker(control.activeBgColor, 1.2) : control.activeBgColor;
            }
            if (control.down) return control.pressedBgColor;
            if (control.hovered) return control.hoverBgColor;
            return control.normalBgColor;
        }
        border.color: {
            if (control.active) return control.activeBorderColor;
            if (control.hovered) return control.hoverBorderColor;
            return control.normalBorderColor;
        }
        border.width: 1
    }

    contentItem: Item {
        anchors.fill: parent

        Canvas {
            id: iconCanvas
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            renderTarget: Canvas.FramebufferObject

            property color currentColor: {
                if (control.active) return control.activeColor;
                if (control.down) return control.pressedColor;
                if (control.hovered) return control.hoverColor;
                return control.normalColor;
            }

            onCurrentColorChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()

            Connections {
                target: control
                function onIconTypeChanged() { iconCanvas.requestPaint(); }
                function onActiveChanged() { iconCanvas.requestPaint(); }
            }

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var w = width;
                var h = height;
                var color = currentColor;

                ctx.strokeStyle = color;
                ctx.fillStyle = color;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";

                if (control.iconType === "refresh") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    var r = w * 0.36;
                    ctx.lineWidth = 2.0;
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, -0.2 * Math.PI, 1.35 * Math.PI, false);
                    ctx.stroke();

                    // Arrowhead at -0.2*PI
                    var ax = cx + r * Math.cos(-0.2 * Math.PI);
                    var ay = cy + r * Math.sin(-0.2 * Math.PI);
                    ctx.beginPath();
                    ctx.moveTo(ax - 2, ay - 5);
                    ctx.lineTo(ax + 5, ay);
                    ctx.lineTo(ax - 3, ay + 3);
                    ctx.closePath();
                    ctx.fill();
                } else if (control.iconType === "add") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    var arm = w * 0.35;
                    ctx.lineWidth = 2.4;
                    ctx.beginPath();
                    ctx.moveTo(cx - arm, cy);
                    ctx.lineTo(cx + arm, cy);
                    ctx.moveTo(cx, cy - arm);
                    ctx.lineTo(cx, cy + arm);
                    ctx.stroke();
                } else if (control.iconType === "capture") {
                    var cx = w * 0.5;
                    var cy = h * 0.72;
                    ctx.lineWidth = 1.8;
                    // Dish / stand base
                    ctx.beginPath();
                    ctx.moveTo(cx - 5, cy + 4);
                    ctx.lineTo(cx + 5, cy + 4);
                    ctx.moveTo(cx, cy);
                    ctx.lineTo(cx, cy + 4);
                    ctx.stroke();

                    // Transmitter center dot
                    ctx.beginPath();
                    ctx.arc(cx, cy - 2, 2.2, 0, 2 * Math.PI);
                    ctx.fill();

                    // Expanding waves
                    ctx.lineWidth = 2.0;
                    ctx.beginPath();
                    ctx.arc(cx, cy - 2, 6.5, -0.8 * Math.PI, -0.2 * Math.PI, false);
                    ctx.stroke();

                    ctx.beginPath();
                    ctx.arc(cx, cy - 2, 11, -0.8 * Math.PI, -0.2 * Math.PI, false);
                    ctx.stroke();
                } else if (control.iconType === "trigger") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    ctx.beginPath();
                    ctx.moveTo(cx + 1, cy - 9);
                    ctx.lineTo(cx - 6, cy + 1);
                    ctx.lineTo(cx - 1, cy + 1);
                    ctx.lineTo(cx - 2, cy + 9);
                    ctx.lineTo(cx + 6, cy - 1);
                    ctx.lineTo(cx + 1, cy - 1);
                    ctx.closePath();
                    ctx.fill();
                } else if (control.iconType === "edit") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    ctx.save();
                    ctx.translate(cx, cy);
                    ctx.rotate(-45 * Math.PI / 180);
                    // Pencil body
                    ctx.fillRect(-2.5, -6, 5, 10);
                    // Tip
                    ctx.beginPath();
                    ctx.moveTo(-2.5, 4);
                    ctx.lineTo(2.5, 4);
                    ctx.lineTo(0, 8.5);
                    ctx.closePath();
                    ctx.fill();
                    // Eraser cap
                    ctx.lineWidth = 1.2;
                    ctx.strokeRect(-2.5, -9, 5, 3);
                    ctx.restore();
                } else if (control.iconType === "transmit") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    // Play / right arrow
                    ctx.beginPath();
                    ctx.moveTo(cx - 5, cy - 7.5);
                    ctx.lineTo(cx + 6.5, cy);
                    ctx.lineTo(cx - 5, cy + 7.5);
                    ctx.closePath();
                    ctx.fill();
                } else if (control.iconType === "close") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    var d = 5;
                    ctx.lineWidth = 2.2;
                    ctx.beginPath();
                    ctx.moveTo(cx - d, cy - d);
                    ctx.lineTo(cx + d, cy + d);
                    ctx.moveTo(cx + d, cy - d);
                    ctx.lineTo(cx - d, cy + d);
                    ctx.stroke();
                } else if (control.iconType === "trash") {
                    var cx = w * 0.5;
                    var cy = h * 0.5;
                    ctx.lineWidth = 1.8;
                    ctx.beginPath();
                    // Lid
                    ctx.moveTo(cx - 6, cy - 4);
                    ctx.lineTo(cx + 6, cy - 4);
                    ctx.moveTo(cx - 2.5, cy - 6);
                    ctx.lineTo(cx + 2.5, cy - 6);
                    // Body
                    ctx.moveTo(cx - 4.5, cy - 2);
                    ctx.lineTo(cx - 3.5, cy + 6);
                    ctx.lineTo(cx + 3.5, cy + 6);
                    ctx.lineTo(cx + 4.5, cy - 2);
                    ctx.stroke();
                }
            }
        }
    }
}
