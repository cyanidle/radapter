import QtQuick 2.7

// Arc gauge widget. A pure-Canvas circular gauge with a needle, value readout,
// label and units. Driven by `value` (resolved from a tag by Node.qml); dims when
// `quality` is comm_fail. All display props come from `spec`.
Item {
    id: gauge
    implicitWidth: 160
    implicitHeight: 160

    property var spec: ({})
    property var value: undefined
    property string quality: "good"

    readonly property real minVal: spec.min !== undefined ? spec.min : 0
    readonly property real maxVal: spec.max !== undefined ? spec.max : 100
    readonly property string label: spec.label !== undefined ? spec.label : ""
    readonly property string units: spec.units !== undefined ? spec.units : ""
    readonly property color arcColor: spec.color !== undefined ? spec.color : "#2196f3"

    // numeric current value clamped into [min, max]; NaN when no data yet
    readonly property real num: value === undefined || value === null ? NaN : Number(value)
    readonly property real frac: isNaN(num) ? 0
        : Math.max(0, Math.min(1, (num - minVal) / (maxVal - minVal || 1)))

    opacity: quality === "comm_fail" ? 0.4 : 1.0

    onFracChanged: arc.requestPaint()
    onArcColorChanged: arc.requestPaint()

    Canvas {
        id: arc
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cx = width / 2, cy = height / 2
            var r = Math.min(width, height) / 2 - 12
            // sweep from 135° to 405° (270° span), clockwise
            var start = Math.PI * 0.75, span = Math.PI * 1.5
            ctx.lineWidth = 12
            ctx.lineCap = "round"
            // track
            ctx.strokeStyle = "#e0e0e0"
            ctx.beginPath()
            ctx.arc(cx, cy, r, start, start + span)
            ctx.stroke()
            // value arc
            ctx.strokeStyle = gauge.arcColor
            ctx.beginPath()
            ctx.arc(cx, cy, r, start, start + span * gauge.frac)
            ctx.stroke()
            // needle
            var ang = start + span * gauge.frac
            ctx.strokeStyle = "#444"
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.lineTo(cx + Math.cos(ang) * (r - 6), cy + Math.sin(ang) * (r - 6))
            ctx.stroke()
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: isNaN(gauge.num) ? "—" : gauge.num.toFixed(1)
            font.pixelSize: Math.max(14, gauge.height / 8)
            font.bold: true
            color: "#222"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: gauge.units
            font.pixelSize: 11
            color: "#888"
            visible: gauge.units.length > 0
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        text: gauge.label
        font.pixelSize: 12
        color: "#555"
        visible: gauge.label.length > 0
    }
}
