import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// Hosts a center content item plus DockablePanel children. Each panel docks to the left,
// right or bottom edge — where several panels stack and each can be minimized so its siblings
// expand (VSCode style) — or floats into its own window. Panels are moved/reordered by
// dragging their header: a ghost follows the cursor, the target edge lights up and an
// insertion line shows where it will land; release over an edge docks/reorders, over the
// middle leaves it put, outside floats it.
//
// The edge slots themselves are stable SplitView children (only their visibility toggles, so
// the outer resize stays correct); the panels within a slot are positioned manually, because
// a SplitView leaves stale gaps when a child is reparented out to another edge.
Item {
    id: host
    property var appWindow: null
    property Item centerContent: null
    property var panels: []               // authoritative order; per-edge order is its subsequence
    property string dropZone: ""          // "", "left", "right", "bottom", "out" while dragging
    property int dropIndex: 0             // insertion index within dropZone's stack
    property var draggingPanel: null      // the panel whose header is being dragged
    property real ghostX: 0               // cursor position (host coords) for the drag ghost
    property real ghostY: 0
    readonly property int headerH: 28

    Component.onCompleted: {
        var ps = []
        for (var i = 0; i < children.length; i++)
            if (children[i].isDockPanel) ps.push(children[i])
        panels = ps
        for (var j = 0; j < ps.length; j++) ps[j].host = host
        if (centerContent) { centerContent.parent = centerSlot; centerContent.anchors.fill = centerSlot }
        relayout()
    }

    function panelsOn(side) {
        var r = []
        for (var i = 0; i < panels.length; i++) if (panels[i].side === side) r.push(panels[i])
        return r
    }
    function slotFor(side) {
        return side === "left" ? leftSlot : side === "right" ? rightSlot : bottomSlot
    }

    // ── layout ──────────────────────────────────────────────────────────────
    // a bottom dock with everything minimized shrinks to a single header strip (bound on the
    // slot's SplitView.maximumHeight, below) so it doesn't leave a tall empty band — the
    // panels keep full width and show their titles
    property bool bottomCollapsed: false
    // SplitView keeps a child's size and ignores later min/max changes; toggling the slot's
    // visibility forces it to re-measure and honor the new clamp when collapse state flips
    onBottomCollapsedChanged: {
        if (panelsOn("bottom").length === 0) return
        bottomSlot.visible = false
        Qt.callLater(function () { bottomSlot.visible = panelsOn("bottom").length > 0 })
    }

    function relayout() {
        bottomCollapsed = allMinimizedOn("bottom")
        layoutEdge("left");  layoutEdge("right");  layoutEdge("bottom")
        leftSlot.visible   = panelsOn("left").length > 0
        rightSlot.visible  = panelsOn("right").length > 0
        bottomSlot.visible = panelsOn("bottom").length > 0
    }
    function allMinimizedOn(side) {
        var l = panelsOn(side)
        if (l.length === 0) return false
        for (var i = 0; i < l.length; i++) if (!l[i].minimized) return false
        return true
    }
    function layoutEdge(side) {
        var slot = slotFor(side)
        var list = panelsOn(side)
        var horizontal = (side === "bottom")
        // reparent + clear anchors, count minimized
        var nMin = 0
        for (var i = 0; i < list.length; i++) {
            var p = list[i]
            p.anchors.fill = undefined
            if (p.parent !== slot) p.parent = slot
            p.visible = true
            if (p.minimized) nMin++
        }
        var nExp = list.length - nMin
        if (horizontal) {
            // bottom: panels share the width equally; minimizing collapses HEIGHT (the header
            // is a horizontal bar, so collapsing width would hide the title) — a minimized
            // panel becomes a full-width header strip at the top of its column
            var eachW = list.length > 0 ? slot.width / list.length : 0
            var x = 0
            for (var a = 0; a < list.length; a++) {
                var pa = list[a]
                pa.x = x; pa.width = eachW
                pa.y = 0
                pa.height = pa.minimized ? headerH : slot.height
                x += eachW
            }
        } else {
            var freeH = Math.max(0, slot.height - nMin * headerH)
            var eachH = nExp > 0 ? freeH / nExp : 0
            var y = 0
            for (var b = 0; b < list.length; b++) {
                var pb = list[b]
                pb.x = 0; pb.width = slot.width
                pb.y = y
                pb.height = pb.minimized ? headerH : eachH
                y += pb.height
            }
        }
    }

    // ── move / dock / float ───────────────────────────────────────────────
    // place `panel` on `side` so it becomes that edge's panel #edgeIndex (clamped).
    function movePanel(panel, side, edgeIndex) {
        var arr = []
        for (var i = 0; i < panels.length; i++) if (panels[i] !== panel) arr.push(panels[i])
        panel.side = side
        var slots = []
        for (var j = 0; j < arr.length; j++) if (arr[j].side === side) slots.push(j)
        var at
        if (slots.length === 0)              at = arr.length
        else if (edgeIndex >= slots.length)  at = slots[slots.length - 1] + 1
        else                                 at = slots[Math.max(0, edgeIndex)]
        arr.splice(at, 0, panel)
        panels = arr
        relayout()
    }
    function dock(panel, side) { movePanel(panel, side, 1e9); closeFloat(panel) }
    function closeFloat(panel) {
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
    function beginPanelDrag(panel) { draggingPanel = panel; dropZone = ""; dropIndex = 0 }
    function updatePanelDrag(gx, gy) {
        if (!draggingPanel) return
        var lp = mapFromGlobal(gx, gy)
        ghostX = lp.x; ghostY = lp.y
        dropZone = zoneForPoint(lp.x, lp.y)
        dropIndex = (dropZone === "left" || dropZone === "right" || dropZone === "bottom")
                    ? dropIndexFor(dropZone) : 0
    }
    function overSlot(slot, x, y) {
        if (!slot.visible) return false
        var p = slot.mapFromItem(host, x, y)
        return p.x >= 0 && p.y >= 0 && p.x <= slot.width && p.y <= slot.height
    }
    function zoneForPoint(x, y) {
        if (x < -50 || y < -50 || x > width + 50 || y > height + 50) return "out"
        // over an existing edge stack -> that edge (enables reorder within it)
        if (overSlot(rightSlot, x, y))  return "right"
        if (overSlot(leftSlot, x, y))   return "left"
        if (overSlot(bottomSlot, x, y)) return "bottom"
        // else an edge band creates / moves to a new edge
        var band = 90
        if (y > height - band) return "bottom"
        if (x < band) return "left"
        if (x > width - band) return "right"
        return ""
    }
    // insertion index within `side`'s stack for the current cursor position (excludes the
    // panel being dragged so the math is the post-removal index movePanel expects)
    function dropIndexFor(side) {
        var slot = slotFor(side)
        var lp = slot.mapFromItem(host, ghostX, ghostY)
        var list = []
        var on = panelsOn(side)
        for (var i = 0; i < on.length; i++) if (on[i] !== draggingPanel) list.push(on[i])
        var idx = list.length
        for (var j = 0; j < list.length; j++) {
            var p = list[j]
            var mid = (side === "bottom") ? (p.x + p.width / 2) : (p.y + p.height / 2)
            var c   = (side === "bottom") ? lp.x : lp.y
            if (c < mid) { idx = j; break }
        }
        return idx
    }
    function endPanelDrag() {
        var p = draggingPanel, z = dropZone, idx = dropIndex
        draggingPanel = null
        dropZone = ""
        if (!p) return
        if (z === "left" || z === "right" || z === "bottom") { movePanel(p, z, idx); closeFloat(p) }
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
    Timer {
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

            Item {
                id: leftSlot; visible: false
                SplitView.preferredWidth: 300; SplitView.minimumWidth: 170
                onWidthChanged: host.layoutEdge("left"); onHeightChanged: host.layoutEdge("left")
            }
            Item { id: centerSlot; SplitView.fillWidth: true; SplitView.minimumWidth: 220 }
            Item {
                id: rightSlot; visible: false
                SplitView.preferredWidth: 320; SplitView.minimumWidth: 200
                onWidthChanged: host.layoutEdge("right"); onHeightChanged: host.layoutEdge("right")
            }
        }
        Item {
            id: bottomSlot; visible: false
            SplitView.preferredHeight: host.bottomCollapsed ? host.headerH : 240
            SplitView.minimumHeight:   host.bottomCollapsed ? host.headerH : 110
            SplitView.maximumHeight:   host.bottomCollapsed ? host.headerH : 1000000
            onWidthChanged: host.layoutEdge("bottom"); onHeightChanged: host.layoutEdge("bottom")
        }
    }

    // translucent preview of the target edge
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
