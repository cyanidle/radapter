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

    // Tick mark step (absolute value). Default: 1/10 of max range.
    property real step: spec.step !== undefined ? spec.step : ((spec.max !== undefined ? spec.max : 100) - (spec.min !== undefined ? spec.min : 0)) / 10

    readonly property real minVal: spec.min !== undefined ? spec.min : 0
    readonly property real maxVal: spec.max !== undefined ? spec.max : 100
    readonly property string label: spec.label !== undefined ? spec.label : ""
    readonly property string units: spec.units !== undefined ? spec.units : ""
    readonly property color arcColor: spec.color !== undefined ? spec.color : "#2196f3"

    // numeric current value clamped into [min, max]; NaN when no data yet
    readonly property real num: value === undefined || value === null ? NaN : Number(value)
    readonly property real frac: isNaN(num) ? 0
        : Math.max(0, Math.min(1, (num - minVal) / (maxVal - minVal || 1)))

    // scale factor for responsive sizing (base size 160)
    readonly property real scale: Math.min(width, height) / 160

    opacity: quality === "comm_fail" ? 0.4 : 1.0

    onFracChanged: arc.requestPaint()

    Canvas {
        id: arc
        anchors.fill: parent
        clip: true
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cx = width / 2, cy = height / 2
            var base = Math.min(width, height) / 2
            var r = base - 12 * gauge.scale
            // sweep from 135° to 405° (270° span), clockwise
            var start = Math.PI * 0.75, span = Math.PI * 1.5
            ctx.lineWidth = 12 * gauge.scale
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
            // tick marks (outside the arc, extending inward)
            var range = gauge.maxVal - gauge.minVal
            var tickStep = gauge.step > 0 ? gauge.step : range / 10
            var majorEvery = tickStep < 1 ? 5 : (tickStep < 5 ? 2 : 1)
            var tickArcR = r + 6 * gauge.scale  // ticks start at arc outer edge
            var startVal = Math.ceil(gauge.minVal / tickStep) * tickStep
            for (var v = startVal; v <= gauge.maxVal + tickStep * 0.01; v += tickStep) {
                var frac = (v - gauge.minVal) / range
                var ang = start + span * frac
                var isMajor = Math.round(v / tickStep) % majorEvery === 0
                var tickLen = isMajor ? 14 * gauge.scale : 8 * gauge.scale
                var tickW = isMajor ? 1.5 * gauge.scale : 1 * gauge.scale
                ctx.strokeStyle = isMajor ? "#555" : "#aaa"
                ctx.lineWidth = tickW
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(ang) * (tickArcR - tickLen), cy + Math.sin(ang) * (tickArcR - tickLen))
                ctx.lineTo(cx + Math.cos(ang) * tickArcR, cy + Math.sin(ang) * tickArcR)
                ctx.stroke()
            }
            // tick labels (on major ticks only) — placed inside the arc
            var labelR = r - 22 * gauge.scale
            ctx.fillStyle = "#444"
            ctx.font = Math.round(10 * gauge.scale) + "px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            for (var v = startVal; v <= gauge.maxVal + tickStep * 0.01; v += tickStep) {
                if (Math.round(v / tickStep) % majorEvery !== 0) continue
                var frac = (v - gauge.minVal) / range
                var ang = start + span * frac
                var lblX = cx + Math.cos(ang) * labelR
                var lblY = cy + Math.sin(ang) * labelR
                var label = (v === Math.floor(v)) ? v.toFixed(0) : v.toFixed(1)
                ctx.fillText(label, lblX, lblY)
            }
            // needle
            var ang = start + span * gauge.frac
            ctx.strokeStyle = "#444"
            ctx.lineWidth = 2 * gauge.scale
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.lineTo(cx + Math.cos(ang) * (r - 6 * gauge.scale), cy + Math.sin(ang) * (r - 6 * gauge.scale))
            ctx.stroke()
        }
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: arc.height / 6
        spacing: 2 * gauge.scale
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: isNaN(gauge.num) ? "—"
                : (gauge.num === Math.floor(gauge.num) ? gauge.num.toFixed(0) : gauge.num.toFixed(1))
            font.pixelSize: Math.max(14 * gauge.scale, gauge.height / 8)
            font.bold: true
            color: "#222"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: gauge.units
            font.pixelSize: 11 * gauge.scale
            color: "#888"
            visible: gauge.units.length > 0
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8 * gauge.scale
        text: gauge.label
        font.pixelSize: 12 * gauge.scale
        color: "#555"
        visible: gauge.label.length > 0
    }
}
