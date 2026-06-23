import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// Hosts a center content item plus DockablePanel children that can be docked to the left,
// right or bottom edge (resizable panes that push the center) or floated into their own
// windows. Dragging a floating window to an edge — or the panel header's dock buttons —
// re-docks it. Empty dock slots collapse automatically.
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
    property string dropZone: ""          // "", "left", "right", "bottom" — active while dragging a float

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
    function slotFor(side) {
        return side === "left" ? leftSlot : side === "right" ? rightSlot : bottomSlot
    }
    function relayout() {
        for (var i = 0; i < panels.length; i++) {
            var p = panels[i]
            if (p.side === "float") continue
            p.parent = slotFor(p.side)
            p.visible = true
        }
        leftSlot.visible = panelsOn("left").length > 0
        rightSlot.visible = panelsOn("right").length > 0
        bottomSlot.visible = panelsOn("bottom").length > 0
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
        panel.visible = true
        win.show(); win.raise(); win.requestActivate()
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
            if (host.dropZone === "") return
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

            Item { id: leftSlot;   visible: false; SplitView.preferredWidth: 300; SplitView.minimumWidth: 170 }
            Item { id: centerSlot; SplitView.fillWidth: true; SplitView.minimumWidth: 220 }
            Item { id: rightSlot;  visible: false; SplitView.preferredWidth: 300; SplitView.minimumWidth: 170 }
        }
        Item { id: bottomSlot; visible: false; SplitView.preferredHeight: 240; SplitView.minimumHeight: 110 }
    }

    // translucent preview of where a dragged float window would dock
    Rectangle {
        visible: host.dropZone !== ""
        z: 9999
        color: "#332196f3"
        border.color: "#2196f3"; border.width: 2
        x: host.dropZone === "right" ? host.width - width : 0
        y: host.dropZone === "bottom" ? host.height - height : 0
        width: host.dropZone === "bottom" ? host.width : host.width * 0.32
        height: host.dropZone === "bottom" ? host.height * 0.4 : host.height
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
