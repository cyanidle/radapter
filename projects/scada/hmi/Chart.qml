import QtQuick 2.7
import QtQuick.Controls 2.13

// Time-series line chart. Collects timestamped values on `value` changes (run mode)
// and draws them on a Canvas with auto-scaling Y axis and a fixed time window (spec.timeFrame,
// default 3600 s = 1 hour). In design mode it renders a representative sine curve.
Item {
    id: chart
    implicitWidth: 320
    implicitHeight: 180

    property var spec: ({})
    property var value: undefined
    property string quality: "good"
    property string mode: "design"    // set by Node.qml: "run" | "design"

    // Configurable time window in seconds; default 1 hour
    readonly property int timeFrame: spec.timeFrame !== undefined ? Number(spec.timeFrame) : 3600
    // Optional fixed Y range; when absent the chart auto-scales
    readonly property var yMinFixed: spec.yMin
    readonly property var yMaxFixed: spec.yMax
    // Display
    readonly property string label: spec.label !== undefined ? spec.label : ""
    readonly property string units: spec.units !== undefined ? spec.units : ""
    readonly property color lineColor: spec.color !== undefined ? spec.color : "#2196f3"

    // Internal circular buffer: array of {t: <epoch ms>, v: <number>}
    property var _data: []
    // Last value we recorded, so we only push on real changes
    property var _lastValue: undefined

    // Padding inside the canvas (left for Y labels, bottom for X labels)
    readonly property int padLeft: 48
    readonly property int padRight: 12
    readonly property int padTop: 8
    readonly property int padBottom: 28

    readonly property int plotW: width - padLeft - padRight
    readonly property int plotH: height - padTop - padBottom

    opacity: quality === "comm_fail" ? 0.4 : 1.0

    // ── data collection (run mode only) ──────────────────────────────────────
    Timer {
        id: flusher
        interval: 1000; repeat: false
        running: false
        onTriggered: chart._flush()
    }

    // Scrolls the (time) X axis and prunes stale points even while the value is steady,
    // so the plot keeps advancing in run mode. Repaints once per second.
    Timer {
        id: ticker
        interval: 1000; repeat: true
        running: chart._mode === "run"
        onTriggered: { chart._prune(); canvas.requestPaint() }
    }

    // On every value change, schedule a flush if in run mode and value is a number
    onValueChanged: {
        if (chart._mode !== "run") return
        var v = Number(chart.value)
        if (isNaN(v)) return
        chart._lastValue = v
        if (!flusher.running) flusher.start()
    }

    // Repaint the design-mode preview when the configured window or display props change.
    onTimeFrameChanged: canvas.requestPaint()
    onModeChanged: canvas.requestPaint()
    onSpecChanged: canvas.requestPaint()

    function _prune() {
        var cutoff = Date.now() - chart.timeFrame * 1000
        while (chart._data.length > 0 && chart._data[0].t < cutoff)
            chart._data.shift()
    }

    function _flush() {
        chart._data.push({ t: Date.now(), v: Number(chart._lastValue) })
        chart._prune()
        canvas.requestPaint()
    }

    // ── design-mode sample data ──────────────────────────────────────────────
    function designPoints() {
        var pts = []
        var now = Date.now()
        var n = 60
        for (var i = 0; i <= n; i++) {
            var frac = i / n
            var t = now - chart.timeFrame * 1000 * (1 - frac)
            var v = 50 + 30 * Math.sin(frac * Math.PI * 4) + 10 * Math.cos(frac * Math.PI * 7)
            pts.push({ t: t, v: v })
        }
        return pts
    }

    // ── painting ─────────────────────────────────────────────────────────────
    function getDataPoints() {
        if (chart._mode === "design")
            return chart.designPoints()
        return chart._data
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var pts = chart.getDataPoints()

            // Determine Y range
            var yMin, yMax
            if (chart.yMinFixed !== undefined && chart.yMaxFixed !== undefined) {
                yMin = Number(chart.yMinFixed)
                yMax = Number(chart.yMaxFixed)
            } else {
                yMin = Infinity; yMax = -Infinity
                for (var i = 0; i < pts.length; i++) {
                    if (pts[i].v < yMin) yMin = pts[i].v
                    if (pts[i].v > yMax) yMax = pts[i].v
                }
                if (!isFinite(yMin) || !isFinite(yMax)) { yMin = 0; yMax = 100 }
                var range = yMax - yMin
                if (range === 0) { yMin -= 5; yMax += 5 }
                else { yMin -= range * 0.05; yMax += range * 0.05 }
            }

            var now = Date.now()
            var tMin = now - chart.timeFrame * 1000
            var tMax = now
            var tRange = chart.timeFrame * 1000

            // Background
            ctx.fillStyle = "#fafafa"
            ctx.fillRect(0, 0, width, height)

            // Plot area
            var px = chart.padLeft
            var py = chart.padTop
            var pw = chart.plotW
            var ph = chart.plotH

            ctx.fillStyle = "#ffffff"
            ctx.strokeStyle = "#e0e0e0"
            ctx.lineWidth = 1
            ctx.fillRect(px, py, pw, ph)
            ctx.strokeRect(px, py, pw, ph)

            // Helper: convert data coords to canvas coords
            function toX(t) { return px + ((t - tMin) / tRange) * pw }
            function toY(v) { return py + ph - ((v - yMin) / (yMax - yMin)) * ph }

            // Grid lines and Y labels
            ctx.strokeStyle = "#eee"
            ctx.lineWidth = 1
            ctx.fillStyle = "#999"
            ctx.font = "9px sans-serif"
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"

            var ySteps = 5
            for (var i = 0; i <= ySteps; i++) {
                var v = yMin + (yMax - yMin) * i / ySteps
                var yy = toY(v)
                ctx.beginPath()
                ctx.moveTo(px, yy)
                ctx.lineTo(px + pw, yy)
                ctx.stroke()
                ctx.fillText(formatNum(v), px - 4, yy)
            }

            // X axis labels (time)
            ctx.textAlign = "center"
            ctx.textBaseline = "top"
            var xCount = Math.min(6, Math.floor(pw / 50))
            for (var i = 0; i <= xCount; i++) {
                var tt = tMin + tRange * i / xCount
                var xx = toX(tt)
                ctx.fillText(formatTime(tt, now), xx, py + ph + 4)
            }

            // Draw the line
            if (pts.length > 1) {
                ctx.beginPath()
                ctx.strokeStyle = chart.lineColor
                ctx.lineWidth = 2
                ctx.moveTo(toX(pts[0].t), toY(pts[0].v))
                for (var i = 1; i < pts.length; i++)
                    ctx.lineTo(toX(pts[i].t), toY(pts[i].v))
                ctx.stroke()

                // Fill under the line
                ctx.lineTo(toX(pts[pts.length - 1].t), py + ph)
                ctx.lineTo(toX(pts[0].t), py + ph)
                ctx.closePath()
                ctx.fillStyle = chart.lineColor + "18"  // ~10% opacity
                ctx.fill()
            } else if (pts.length === 1) {
                // Single point — draw a dot
                ctx.beginPath()
                ctx.arc(toX(pts[0].t), toY(pts[0].v), 3, 0, Math.PI * 2)
                ctx.fillStyle = chart.lineColor
                ctx.fill()
            }

            // Current value label at the right edge
            if (pts.length > 0) {
                var last = pts[pts.length - 1]
                var lx = px + pw + 4
                var ly = toY(last.v)
                ctx.fillStyle = chart.lineColor
                ctx.font = "bold 10px sans-serif"
                ctx.textAlign = "left"
                ctx.textBaseline = "middle"
                ctx.fillText(formatNum(last.v), lx, ly)
            }
        }
    }

    // Label at the top-left
    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 4
        text: chart.label
        font.pixelSize: 11
        font.bold: true
        color: "#555"
        visible: chart.label.length > 0
    }

    // Units at the top-right
    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        text: chart.units
        font.pixelSize: 10
        color: "#999"
        visible: chart.units.length > 0
    }

    // ── formatting helpers ───────────────────────────────────────────────────
    function formatNum(v) {
        if (v === Math.floor(v)) return v.toFixed(0)
        return v.toFixed(1)
    }

    function formatTime(ts, now) {
        var d = new Date(ts)
        var h = d.getHours()
        var m = d.getMinutes()
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m
    }

    // Expose mode so onValueChanged knows when to collect
    readonly property string _mode: chart.mode
}