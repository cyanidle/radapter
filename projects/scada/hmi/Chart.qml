import QtQuick 2.7
import QtCharts 2.15

// Time-series line chart built on QtCharts. Collects timestamped values on `value`
// changes (run mode) into a LineSeries drawn against a scrolling DateTimeAxis and an
// auto- or fixed-range ValueAxis (spec.yMin/spec.yMax). The time window is spec.timeFrame
// (default 3600 s = 1 hour). In design mode it renders a representative sine curve.
//
// QtCharts renders through QtWidgets' QGraphicsScene, so the radapter GUI build runs on a
// QApplication (see app/main.cpp) — required for this component to load.
Item {
    id: chart
    implicitWidth: 320
    implicitHeight: 180

    property var spec: ({})
    property var value: undefined
    property string quality: "good"
    property string mode: "design"    // set by Node.qml: "run" | "design"

    readonly property int timeFrame: spec.timeFrame !== undefined ? Number(spec.timeFrame) : 3600
    readonly property var yMinFixed: spec.yMin
    readonly property var yMaxFixed: spec.yMax
    readonly property string label: spec.label !== undefined ? spec.label : ""
    readonly property string units: spec.units !== undefined ? spec.units : ""
    readonly property color lineColor: spec.color !== undefined ? spec.color : "#2196f3"

    // last value seen since the previous flush, so bursts coalesce to one point per second
    property var _lastValue: undefined

    opacity: quality === "comm_fail" ? 0.4 : 1.0

    // ── data collection (run mode only) ──────────────────────────────────────
    Timer {
        id: flusher
        interval: 1000; repeat: false; running: false
        onTriggered: chart._flush()
    }
    // Scrolls the X axis and prunes stale points even while the value is steady.
    Timer {
        id: ticker
        interval: 1000; repeat: true
        running: chart.mode === "run"
        onTriggered: { chart._prune(); chart._scrollAxes(); chart._rescaleY() }
    }

    onValueChanged: {
        if (chart.mode !== "run") return
        var v = Number(chart.value)
        if (isNaN(v)) return
        chart._lastValue = v
        if (!flusher.running) flusher.start()
    }

    onModeChanged: chart._reload()
    onSpecChanged: chart._reload()
    Component.onCompleted: chart._reload()

    function _now() { return Date.now() }

    function _scrollAxes() {
        var now = chart._now()
        axisX.min = new Date(now - chart.timeFrame * 1000)
        axisX.max = new Date(now)
    }

    function _prune() {
        var cutoff = chart._now() - chart.timeFrame * 1000
        while (series.count > 0 && series.at(0).x < cutoff) series.remove(0)
    }

    function _rescaleY() {
        if (chart.yMinFixed !== undefined && chart.yMaxFixed !== undefined) {
            axisY.min = Number(chart.yMinFixed)
            axisY.max = Number(chart.yMaxFixed)
            return
        }
        var lo = Infinity, hi = -Infinity
        for (var i = 0; i < series.count; i++) {
            var v = series.at(i).y
            if (v < lo) lo = v
            if (v > hi) hi = v
        }
        if (!isFinite(lo) || !isFinite(hi)) { lo = 0; hi = 100 }
        var range = hi - lo
        if (range === 0) { lo -= 5; hi += 5 }
        else { lo -= range * 0.05; hi += range * 0.05 }
        axisY.min = lo
        axisY.max = hi
    }

    function _flush() {
        series.append(chart._now(), Number(chart._lastValue))
        chart._prune()
        chart._scrollAxes()
        chart._rescaleY()
    }

    // Rebuild the series from scratch (mode/spec change). Design mode seeds a sine curve.
    function _reload() {
        series.clear()
        if (chart.mode === "design") {
            var now = chart._now()
            var n = 60
            for (var i = 0; i <= n; i++) {
                var frac = i / n
                var t = now - chart.timeFrame * 1000 * (1 - frac)
                var v = 50 + 30 * Math.sin(frac * Math.PI * 4) + 10 * Math.cos(frac * Math.PI * 7)
                series.append(t, v)
            }
        }
        _scrollAxes()
        _rescaleY()
    }

    ChartView {
        id: view
        anchors.fill: parent
        antialiasing: true
        legend.visible: false
        backgroundColor: "#fafafa"
        title: chart.label
        titleColor: "#555"
        titleFont.pixelSize: 12
        titleFont.bold: true
        margins.top: 4; margins.bottom: 4; margins.left: 4; margins.right: 4

        LineSeries {
            id: series
            color: chart.lineColor
            width: 2
            axisX: DateTimeAxis {
                id: axisX
                format: "hh:mm"
                labelsFont.pixelSize: 9
                tickCount: 5
                gridLineColor: "#eee"
            }
            axisY: ValueAxis {
                id: axisY
                labelsFont.pixelSize: 9
                gridLineColor: "#eee"
            }
        }
    }

    // Units, top-right
    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        text: chart.units
        font.pixelSize: 10
        color: "#999"
        visible: chart.units.length > 0
    }
}
