import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// Hosts a center content item plus DockablePanel children. Each panel docks to the left,
// right or bottom edge — where several panels stack in a resizable SplitView and each can be
// minimized so its siblings expand (VSCode style) — or floats into its own window. Panels
// are moved by dragging their header: a ghost follows the cursor and the target edge lights
// up; release over an edge docks, over the middle leaves it put, outside floats it.
//
// Usage: set `centerContent` to the main item and declare DockablePanel children:
//   DockHost { appWindow: root; centerContent: graph
//       Item { id: graph; anchors.fill: parent ? parent : undefined ... }
//       DockablePanel { side: "right"; title: "Properties" ... } }
Item {
    id: host
    property var appWindow: null
    property Item centerContent: null
    property var panels: []
    property string dropZone: ""          // "", "left", "right", "bottom", "out" while dragging
    property var draggingPanel: null      // the panel whose header is being dragged
    property real ghostX: 0               // cursor position (host coords) for the drag ghost
    property real ghostY: 0

    Component.onCompleted: {
        var ps = []
        for (var i = 0; i < children.length; i++)
            if (children[i].isDockPanel) ps.push(children[i])
        panels = ps
        for (var j = 0; j < ps.length; j++) ps[j].host = host
        if (centerContent) centerContent.parent = centerSlot
        relayout()
    }

    function panelsOn(side) {
        var r = []
        for (var i = 0; i < panels.length; i++) if (panels[i].side === side) r.push(panels[i])
        return r
    }
    function sideContainer(side) {
        return side === "left" ? leftSide : side === "right" ? rightSide : bottomSide
    }
    function relayout() {
        for (var i = 0; i < panels.length; i++) {
            var p = panels[i]
            if (p.side === "float") continue
            p.anchors.fill = undefined          // the edge SplitView owns docked geometry
            p.parent = sideContainer(p.side)
            p.visible = true
        }
        leftSide.visible = panelsOn("left").length > 0
        rightSide.visible = panelsOn("right").length > 0
        bottomSide.visible = panelsOn("bottom").length > 0
    }

    // dock a panel to an edge. Reparent it out of any float window FIRST, then close the
    // (now empty) window — otherwise destroying the window would take the panel with it.
    function dock(panel, side) {
        panel.side = side
        dropZone = ""
        relayout()
        if (panel._win) { var w = panel._win; panel._win = null; w.close() }
    }
    function floatPanel(panel) {
        panel.side = "float"
        relayout()
        var win = floatComp.createObject(null, { panel: panel })
        panel._win = win
        if (appWindow) { win.x = appWindow.x + 140; win.y = appWindow.y + 120 }
        panel.parent = win.body
        panel.anchors.fill = win.body
        panel.visible = true
        win.show(); win.raise(); win.requestActivate()
    }

    // ── drag-and-drop docking via a panel's header ─────────────────────────
    // The panel header reports the global cursor; we map it into host coordinates, light up
    // the edge zone under it and move a ghost there. Releasing over an edge docks; over the
    // middle leaves the panel put; outside the host floats it.
    function beginPanelDrag(panel) { draggingPanel = panel; dropZone = "" }
    function updatePanelDrag(gx, gy) {
        if (!draggingPanel) return
        var lp = mapFromGlobal(gx, gy)
        ghostX = lp.x; ghostY = lp.y
        dropZone = zoneForPoint(lp.x, lp.y)
    }
    function zoneForPoint(x, y) {
        var band = 90
        if (x < -50 || y < -50 || x > width + 50 || y > height + 50) return "out"
        if (y > height - band) return "bottom"
        if (x < band) return "left"
        if (x > width - band) return "right"
        return ""
    }
    function endPanelDrag() {
        var p = draggingPanel, z = dropZone
        draggingPanel = null
        dropZone = ""
        if (!p) return
        if (z === "left" || z === "right" || z === "bottom") dock(p, z)
        else if (z === "out" && p.side !== "float") floatPanel(p)
        // z === "" → middle: leave the panel where it is
    }

    // ── drag-to-edge snapping for a floating window ─────────────────────────
    function updateDrag(win) {
        if (!win.tracking) return
        dropZone = zoneFor(win)
        snapTimer.restart()
    }
    function zoneFor(win) {
        var w = appWindow
        if (!w) return ""
        var band = 90
        var cx = win.x + win.width / 2, cy = win.y + win.height / 2
        if (cx < w.x - band || cx > w.x + w.width + band) return ""
        if (cy < w.y - band || cy > w.y + w.height + band) return ""
        if (win.y + win.height >= w.y + w.height - band) return "bottom"
        if (win.x <= w.x + band) return "left"
        if (win.x + win.width >= w.x + w.width - band) return "right"
        return ""
    }
    Timer {                              // dock once the window has rested briefly in a zone
        id: snapTimer
        interval: 350
        onTriggered: {
            if (host.dropZone === "" || host.dropZone === "out") return
            var fp = host.panelsOn("float")
            if (fp.length) host.dock(fp[0], host.dropZone)
        }
    }

    SplitView {
        id: vsplit
        anchors.fill: parent
        orientation: Qt.Vertical

        SplitView {
            id: hsplit
            orientation: Qt.Horizontal
            SplitView.fillHeight: true

            SplitView {   // left edge: panels stack top-to-bottom
                id: leftSide
                orientation: Qt.Vertical
                visible: false
                SplitView.preferredWidth: 300; SplitView.minimumWidth: 170
            }
            Item { id: centerSlot; SplitView.fillWidth: true; SplitView.minimumWidth: 220 }
            SplitView {   // right edge
                id: rightSide
                orientation: Qt.Vertical
                visible: false
                SplitView.preferredWidth: 320; SplitView.minimumWidth: 200
            }
        }
        SplitView {       // bottom edge: panels stack left-to-right
            id: bottomSide
            orientation: Qt.Horizontal
            visible: false
            SplitView.preferredHeight: 240; SplitView.minimumHeight: 110
        }
    }

    // translucent preview of where a dragged panel/window would dock
    Rectangle {
        visible: host.dropZone === "left" || host.dropZone === "right" || host.dropZone === "bottom"
        z: 9998
        color: "#332196f3"
        border.color: "#2196f3"; border.width: 2
        x: host.dropZone === "right" ? host.width - width : 0
        y: host.dropZone === "bottom" ? host.height - height : 0
        width: host.dropZone === "bottom" ? host.width : host.width * 0.32
        height: host.dropZone === "bottom" ? host.height * 0.4 : host.height
    }

    // a small ghost that follows the cursor while a panel header is being dragged
    Rectangle {
        visible: host.draggingPanel !== null
        z: 9999
        width: 150; height: 28
        radius: 3
        color: "#e3f2fd"
        border.color: "#2196f3"; border.width: 1
        opacity: 0.92
        x: host.ghostX - width / 2
        y: host.ghostY - height / 2
        Label {
            anchors.fill: parent
            anchors.leftMargin: 8; anchors.rightMargin: 8
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: host.draggingPanel ? host.draggingPanel.title : ""
            font.bold: true
        }
    }

    Component {
        id: floatComp
        ApplicationWindow {
            id: win
            property var panel
            property alias body: bodyItem
            property bool tracking: false
            width: 440; height: 540
            title: panel ? panel.title : ""
            // closing the OS window re-docks the panel to its home side (unless it was
            // already re-docked elsewhere, which set side away from "float")
            onClosing: {
                if (win.panel && win.panel.side === "float") host.dock(win.panel, win.panel.homeSide)
                Qt.callLater(win.destroy)
            }
            onXChanged: host.updateDrag(win)
            onYChanged: host.updateDrag(win)
            Component.onCompleted: Qt.callLater(function () { win.tracking = true })
            Item { id: bodyItem; anchors.fill: parent }
        }
    }
}
