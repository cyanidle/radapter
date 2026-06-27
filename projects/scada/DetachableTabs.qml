import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.0

// VSCode-style tab groups. Each group is a column (tab bar + content) inside a
// single horizontal SplitView, which handles resize handles between columns natively.
// Columns are created/destroyed on demand via createObject/destroy — no arbitrary cap.
// Dragging a tab to the left/right third of any column creates a new group on that side;
// dragging between group bars moves it. Empty groups auto-collapse.
Item {
    id: tabs
    onWidthChanged: Qt.callLater(relayout)

    property var appWindow: null
    property var panelList: []
    property string settingsKey: "tabs/main"   // QSettings category for tab layout
    property bool _tabRestored: false

    // ── tab state persistence ───────────────────────────────────────────────
    onGroupsChanged: _scheduleTabSave()

    Settings {
        id: tabCfg
        category: tabs.settingsKey
        property string groupsJson: ""
        property string detachedJson: ""
    }

    Timer {
        id: _tabSaveTimer
        interval: 300
        onTriggered: _persistTabState()
    }

    function _scheduleTabSave() {
        if (!_tabRestored) return
        _tabSaveTimer.restart()
    }

    function _persistTabState() {
        _tabSaveTimer.stop()
        tabCfg.groupsJson = JSON.stringify(groups)
        var det = []
        for (var i = 0; i < panelList.length; i++)
            if (panelList[i].detached) det.push(i)
        tabCfg.detachedJson = JSON.stringify(det)
    }

    function _restoreTabState() {
        if (!tabCfg.groupsJson) return false
        try {
            var savedGroups = JSON.parse(tabCfg.groupsJson)
            if (!savedGroups || !savedGroups.length) return false
            var savedDetached = JSON.parse(tabCfg.detachedJson) || []

            // Validate: panel count must match
            var total = 0
            for (var g = 0; g < savedGroups.length; g++)
                total += savedGroups[g].tabs ? savedGroups[g].tabs.length : 0
            total += savedDetached.length
            if (total !== panelList.length) return false

            // Validate: all indices in range
            for (var g2 = 0; g2 < savedGroups.length; g2++) {
                var tlist = savedGroups[g2].tabs
                if (!tlist) return false
                for (var t = 0; t < tlist.length; t++)
                    if (tlist[t] < 0 || tlist[t] >= panelList.length) return false
                if (savedGroups[g2].active < 0 || savedGroups[g2].active >= panelList.length)
                    savedGroups[g2].active = tlist.length > 0 ? tlist[0] : -1
            }
            for (var d = 0; d < savedDetached.length; d++)
                if (savedDetached[d] < 0 || savedDetached[d] >= panelList.length) return false

            groups = savedGroups
            groups = groups  // force notification
            relayout()

            // Re-detach panels that were detached (WindowSettings handles geometry)
            for (var d2 = 0; d2 < savedDetached.length; d2++) {
                var idx = savedDetached[d2]
                if (!panelList[idx].detached && panelList[idx].detachable)
                    _detachAt(idx, 100, 100)
            }
            return true
        } catch (e) {
            return false
        }
    }

    // ── group model ──────────────────────────────────────────────────────
    property var groups: [{tabs: [], active: -1}]
    readonly property bool isSplit: groups.length >= 2

    property var colItems: []    // created column Items (children of contentSplit)
    property var barLoaders: []  // Loader per column
    property var contentItems: [] // content Item per column

    // ── drag state ───────────────────────────────────────────────────────
    property int dragPanelIdx: -1
    property int dragFromGroup: -1
    property bool dragging: false
    property real dragStartX: 0
    property real dragStartY: 0
    property real dragCurX: 0
    property real dragCurY: 0
    property string dropZone: ""
    property int dropTargetGroup: -1
    property int dropBarIdx: 0

    readonly property int barH: 32
    readonly property int dragThreshold: 6
    readonly property real edgeFrac: 0.40

    // ── column component ─────────────────────────────────────────────────
    Component {
        id: colComp
        Item {
            id: colRoot
            property int groupIdx: -1
            property alias barLoader: barLdr
            property alias content: contentItem
            SplitView.preferredWidth: 400
            SplitView.minimumWidth: 200

            Loader {
                id: barLdr
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: barH; sourceComponent: tabBarComp; active: false
            }
            Item {
                id: contentItem
                anchors { left: parent.left; right: parent.right; top: barLdr.bottom; bottom: parent.bottom }
                clip: true
            }
        }
    }

    // ── collect panels ───────────────────────────────────────────────────
    Component.onCompleted: {
        var arr = []
        for (var i = 0; i < tabs.children.length; i++)
            if (tabs.children[i].isTabPanel) arr.push(tabs.children[i])
        panelList = arr
        var idxs = []
        for (var j = 0; j < arr.length; j++)
            if (!arr[j].detached) idxs.push(j)
        groups = [{tabs: idxs, active: idxs.length > 0 ? idxs[0] : -1}]

        if (!_restoreTabState())
            relayout()
        _tabRestored = true
        _scheduleTabSave()   // persist the initial (or restored) state
    }

    // ── helpers ──────────────────────────────────────────────────────────
    function groupOf(panelIdx) {
        for (var g = 0; g < groups.length; g++)
            if (groups[g].tabs.indexOf(panelIdx) >= 0) return g
        return -1
    }
    function isDocked(panelIdx) { return groupOf(panelIdx) >= 0 }

    // ── layout ───────────────────────────────────────────────────────────
    function relayout() {
        // Hide all docked panels; they'll be re-shown when placed
        for (var i = 0; i < panelList.length; i++)
            if (!panelList[i].detached && isDocked(i)) panelList[i].visible = false

        var n = groups.length
        var availW = contentSplit.width

        // Ensure we have exactly n columns
        while (colItems.length < n) {
            var col = colComp.createObject(contentSplit, {"SplitView.preferredWidth": availW / (colItems.length + 1)})
            colItems.push(col)
            barLoaders.push(col.barLoader)
            contentItems.push(col.content)
        }
        while (colItems.length > n) {
            var last = colItems.pop()
            barLoaders.pop(); contentItems.pop()
            last.destroy()
        }

        // Rebalance widths equally when group count changes
        for (var g = 0; g < n; g++) {
            var colItem = colItems[g]
            if (!colItem) continue
            colItem.groupIdx = g
            colItem.SplitView.preferredWidth = availW / n
            colItem.visible = true

            var bl = barLoaders[g]
            if (bl) {
                if (!bl.item) bl.active = true
                if (bl.item) {
                    bl.item.groupIdx = g
                    bl.item.height = barH
                    bl.item.tabIndices = Qt.binding(function(g2) {
                        return function() {
                            return tabs.groups.length > g2 ? tabs.groups[g2].tabs : []
                        }
                    }(g))
                }
            }
            // Show active panel in this group
            var gg = groups[g]
            if (gg.active >= 0 && gg.active < panelList.length && !panelList[gg.active].detached) {
                var panel = panelList[gg.active]
                if (panel.parent !== contentItems[g]) panel.parent = contentItems[g]
                panel.visible = true
            }
        }
    }

    // ── group mutations ──────────────────────────────────────────────────
    function groupAdd(groupIdx, panelIdx) {
        if (panelIdx < 0 || panelIdx >= panelList.length) return
        if (panelList[panelIdx].detached) return
        var old = groupOf(panelIdx)
        if (old === groupIdx) return
        if (old >= 0) {
            groupRemovePanel(old, panelIdx)
            if (groupIdx > groups.length) groupIdx = groups.length - 1
        }
        if (groupIdx < 0 || groupIdx >= groups.length) {
            groupIdx = groups.length - 1
            if (groupIdx < 0) {
                groups = [{tabs: [panelIdx], active: panelIdx}]; relayout(); return
            }
        }
        var g = groups[groupIdx]
        g.tabs.push(panelIdx)
        g.active = panelIdx
        groups = groups
        relayout()
    }

    function groupRemovePanel(groupIdx, panelIdx) {
        if (groupIdx >= groups.length) return
        var g = groups[groupIdx]
        var oi = g.tabs.indexOf(panelIdx)
        if (oi < 0) return
        g.tabs.splice(oi, 1)
        if (g.active === panelIdx) g.active = g.tabs.length > 0 ? g.tabs[0] : -1
        if (g.tabs.length === 0 && groups.length > 1) {
            // Distribute the empty group's splitter space to neighbours by
            // removing it — remaining items reflow naturally
            groups.splice(groupIdx, 1)
        }
        groups = groups
    }

    function groupActivate(groupIdx, panelIdx) {
        if (groupIdx >= groups.length) return
        if (panelList[panelIdx].detached) return
        var g = groups[groupIdx]
        if (g.tabs.indexOf(panelIdx) < 0) return
        g.active = panelIdx
        groups = groups
        relayout()
    }

    function makeSplit(panelIdx, side, atGroup) {
        if (panelIdx < 0 || panelIdx >= panelList.length) return
        if (panelList[panelIdx].detached) return

        var fromG = groupOf(panelIdx)
        if (fromG < 0) { groupAdd(0, panelIdx); return }
        if (fromG === atGroup && groups[fromG].tabs.length === 1) return

        groupRemovePanel(fromG, panelIdx)
        if (fromG < atGroup && groups.length < atGroup + 1) atGroup--

        var insertAt = side === "left" ? atGroup : atGroup + 1
        if (insertAt < 0) insertAt = 0
        if (insertAt > groups.length) insertAt = groups.length

        groups.splice(insertAt, 0, {tabs: [panelIdx], active: panelIdx})
        groups = groups
        radapter.note("tabs:split|" + panelList[panelIdx].title + "|" + side)
        relayout()
    }

    function moveInGroup(groupIdx, fromIdx, toIdx) {
        if (fromIdx === toIdx) return
        var g = groups[groupIdx]
        var arr = g.tabs.slice()
        arr.splice(fromIdx, 1)
        var ti = toIdx > fromIdx ? toIdx - 1 : toIdx
        arr.splice(ti, 0, g.tabs[fromIdx])
        g.tabs = arr
        groups = groups
        radapter.note("tabs:reordered|" + panelList[g.tabs[ti]].title)
        relayout()
    }

    // ── panel lifecycle ──────────────────────────────────────────────────
    function select(panelIdx) {
        if (panelIdx < 0 || panelIdx >= panelList.length) return
        var panel = panelList[panelIdx]
        if (panel.detached) { _raise(panel); return }
        var g = groupOf(panelIdx)
        var wasActive = g >= 0 && groups[g].active === panelIdx
        if (g >= 0) groupActivate(g, panelIdx)
        else groupAdd(0, panelIdx)
        if (!wasActive && panelList[panelIdx]) radapter.note("tabs:switched|" + panel.title)
    }
    function selectPanel(p) {
        var i = panelList.indexOf(p)
        if (i >= 0) select(i)
    }

    function closePanelIdx(panelIdx) {
        if (panelIdx < 0 || panelIdx >= panelList.length) return
        var p = panelList[panelIdx]
        if (!p.closable) return
        radapter.note("tabs:closed|" + p.title)
        if (p.detached) {
            if (p._win) { var w = p._win; p._win = null; w.close() }
            p.detached = false
            _scheduleTabSave()
            return
        }
        var g = groupOf(panelIdx)
        if (g < 0) return
        groupRemovePanel(g, panelIdx)
        groups = groups
        relayout()
    }

    // ── detach / redock / raise ──────────────────────────────────────────
    function _detachAt(panelIdx, gx, gy) {
        var panel = panelList[panelIdx]
        if (!panel || !panel.detachable) return
        var g = groupOf(panelIdx)
        if (g >= 0) { groupRemovePanel(g, panelIdx); groups = groups }
        var win = floatComp.createObject(null, { panel: panel })
        win.x = gx; win.y = gy
        panel.detached = true; panel._win = win
        panel.parent = win.body; panel.visible = true
        radapter.note("tabs:detached|" + panel.title)
        win.show(); win.raise(); win.requestActivate()
        relayout()
    }
    function _detach(panel, gx, gy) { _detachAt(panelList.indexOf(panel), gx, gy) }
    function _redock(panel) {
        panel.detached = false; panel._win = null
        var idx = panelList.indexOf(panel)
        if (groups.length === 0) groups = [{tabs: [idx], active: idx}]
        else { groups[0].tabs.push(idx); groups[0].active = idx }
        groups = groups
        radapter.note("tabs:redocked|" + panel.title)
        relayout(); select(idx)
    }
    function _raise(panel) {
        if (!panel._win) return
        var w = panel._win
        if (w.visibility === Window.Minimized)
            w.visibility = Window.Windowed
        w.raise()
        w.requestActivate()
    }

    // ── drag handling ────────────────────────────────────────────────────
    function beginDrag(panelIdx, fromGroup, localX, localY) {
        dragPanelIdx = panelIdx; dragFromGroup = fromGroup
        dragStartX = localX; dragStartY = localY
        dragCurX = localX; dragCurY = localY
        dragging = false; dropZone = ""
    }

    function updateDrag(globalX, globalY) {
        if (dragPanelIdx < 0) return
        var lp = tabs.mapFromGlobal(globalX, globalY)
        dragCurX = lp.x; dragCurY = lp.y
        var dx = lp.x - dragStartX, dy = lp.y - dragStartY
        if (!dragging && Math.sqrt(dx * dx + dy * dy) < dragThreshold) return
        dragging = true

        if (isOutsideWindow(globalX, globalY)) { dropZone = "out"; return }

        var detH = detachedLoader.detachedBarH
        if (lp.y < detH) {
            dropZone = "bar"; dropTargetGroup = -1
        } else if (lp.y < detH + barH) {
            dropTargetGroup = groupAtX(lp.x)
            dropZone = "bar"
            dropBarIdx = barInsertLoader(barLoaders[dropTargetGroup], lp.x)
        } else {
            var g = groupAtX(lp.x)
            dropTargetGroup = g
            if (g >= 0 && g < colItems.length) {
                var col = colItems[g]
                if (!col) { dropZone = "content"; return }
                var relX = lp.x - col.x
                var edgeW = col.width * edgeFrac
                if (relX < edgeW && groups.length > 1)
                    dropZone = "split-left"
                else if (relX > col.width - edgeW && groups.length > 1)
                    dropZone = "split-right"
                else if (relX < edgeW)
                    dropZone = "left-edge"
                else if (relX > col.width - edgeW)
                    dropZone = "right-edge"
                else
                    dropZone = "content"
            } else {
                dropZone = "content"
            }
        }
    }

    function endDrag() {
        if (!dragging) {
            if (dragPanelIdx >= 0 && dragPanelIdx < panelList.length && !panelList[dragPanelIdx].detached)
                select(dragPanelIdx)
            dragPanelIdx = -1; dragFromGroup = -1; dropZone = ""
            return
        }
        var idx = dragPanelIdx, fromG = dragFromGroup
        dragging = false; dragPanelIdx = -1; dragFromGroup = -1
        var zone = dropZone; var tg = dropTargetGroup; dropZone = ""
        var title = idx >= 0 && idx < panelList.length ? panelList[idx].title : "?"

        if (zone === "bar") {
            if (tg < 0 || tg >= groups.length) tg = groups.length - 1
            if (tg === fromG) {
                var g2 = groups[tg]
                var fromPos = g2.tabs.indexOf(idx)
                if (fromPos >= 0) { moveInGroup(tg, fromPos, dropBarIdx); radapter.note("tabs:drag_reordered|" + title) }
                else { groupActivate(tg, idx); radapter.note("tabs:drag_moved|" + title + "|group" + tg) }
            } else { groupAdd(tg, idx); radapter.note("tabs:drag_moved|" + title + "|group" + tg) }
        } else if (zone === "left-edge") {
            makeSplit(idx, "left", 0)
        } else if (zone === "right-edge") {
            makeSplit(idx, "right", groups.length - 1)
        } else if (zone === "split-left") {
            makeSplit(idx, "left", tg >= 0 ? tg : 0)
        } else if (zone === "split-right") {
            makeSplit(idx, "right", tg >= 0 ? tg : groups.length - 1)
        } else if (zone === "content") {
            if (isSplit && tg >= 0) { groupAdd(tg, idx); radapter.note("tabs:drag_moved|" + title + "|group" + tg) }
            else select(idx)
        } else if (zone === "out") {
            var p = panelList[idx]
            if (p && p.detachable && !p.detached) {
                var g = tabs.mapToGlobal(dragCurX, dragCurY)
                _detachAt(idx, g.x - 60, g.y - 10)
            }
        }
    }

    function groupAtX(px) {
        for (var g = 0; g < groups.length; g++) {
            var c = colItems[g]
            if (c && c.visible && px >= c.x && px <= c.x + c.width) return g
        }
        return px < tabs.width / 2 ? 0 : groups.length - 1
    }

    function barInsertLoader(loader, px) {
        var item = loader ? loader.item : null
        if (!item) return 0
        var lp = item.mapFromItem(tabs, px, 0)
        var kids = item.visibleChildren, count = 0
        for (var i = 0; i < kids.length; i++) {
            var c = kids[i]
            if (!c || c._isOverlay) continue
            if (lp.x < c.x + c.width / 2) return count; count++
        }
        return count
    }
    function isOutsideWindow(gx, gy) {
        if (!appWindow) return false
        var w = appWindow
        return gx < w.x || gy < w.y || gx > w.x + w.width || gy > w.y + w.height
    }

    // ── float window component ───────────────────────────────────────────
    Component {
        id: floatComp
        ApplicationWindow {
            id: win
            property var panel; property alias body: bodyItem
            width: 820; height: 580
            title: panel ? panel.title : ""
            onClosing: { tabs._redock(panel); Qt.callLater(destroy) }

            WindowSettings {
                key: "floatTab/" + (panel ? panel.title : "unknown")
                win: win
                defaultWidth: 820; defaultHeight: 580
            }

            header: ToolBar {
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    Label { text: win.title; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Button { text: "⤡ Dock back"; onClicked: win.close() }
                }
            }
            Item { id: bodyItem; anchors.fill: parent }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // Tab-bar component
    // ══════════════════════════════════════════════════════════════════════
    Component {
        id: tabBarComp
        Row {
            id: barRow; spacing: 2
            property int groupIdx: -1
            property var tabIndices: []

            Repeater {
                model: barRow.tabIndices
                delegate: Rectangle {
                    id: tab
                    readonly property bool _isOverlay: false
                    objectName: panel ? "tab_" + panel.title : ""
                    width: tabRow2.implicitWidth + 20; height: barRow.height
                    readonly property int panelIdx: barRow.tabIndices[index]
                    readonly property var panel: panelIdx !== undefined && panelIdx < tabs.panelList.length
                                                 ? tabs.panelList[panelIdx] : null
                    readonly property bool isActive: panel && !panel.detached
                                                    && barRow.groupIdx >= 0 && barRow.groupIdx < tabs.groups.length
                                                    && tabs.groups[barRow.groupIdx].active === panelIdx
                    color: panel && panel.detached ? "#f0f0f0"
                           : (isActive ? "#ffffff" : "#e8e8e8")
                    border.color: "#d0d0d0"; border.width: 1

                    Row {
                        id: tabRow2; anchors.centerIn: parent; spacing: 6
                        opacity: panel && panel.detached ? 0.55 : 1.0
                        Text {
                            text: panel ? panel.title : ""
                            font.bold: tab.isActive
                            font.italic: panel && panel.detached
                            color: tab.isActive ? "#1565c0" : "#555"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            visible: panel && panel.detachable
                            text: panel && panel.detached ? "⮌" : "⤢"; color: "#999"
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent; anchors.margins: -4
                                onClicked: {
                                    if (!panel) return
                                    if (panel.detached) { tabs._raise(panel); return }
                                    var g = mapToGlobal(width / 2, height / 2)
                                    tabs._detachAt(panelIdx, g.x + 40, g.y + 40)
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent; z: -1
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        drag.target: tabDragDummy
                        onPressed: {
                            if (mouse.button === Qt.RightButton) { ctxMenu.popup(); return }
                            tabs.beginDrag(panelIdx, barRow.groupIdx, mouse.x, mouse.y)
                        }
                        onPositionChanged: {
                            if (tabs.dragPanelIdx < 0) return
                            var g = mapToGlobal(mouse.x, mouse.y)
                            tabs.updateDrag(g.x, g.y)
                        }
                        onReleased: { if (tabs.dragPanelIdx >= 0) tabs.endDrag() }
                        onClicked: {
                            if (mouse.button === Qt.RightButton) return
                            if (!panel) return
                            if (panel.detached) tabs._raise(panel)
                            else tabs.select(panelIdx)
                        }
                    }
                    Item { id: tabDragDummy; visible: false }

                    Menu {
                        id: ctxMenu
                        MenuItem {
                            text: "Close"
                            enabled: panel && panel.closable
                            onTriggered: tabs.closePanelIdx(panelIdx)
                        }
                    }
                }
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // Visual tree
    // ══════════════════════════════════════════════════════════════════════

    // ── detached tab ghosts ───────────────────────────────────────────
    Loader {
        id: detachedLoader
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: detachedBarH; active: detachedCount > 0
        sourceComponent: tabBarComp
        readonly property int detachedCount: {
            var n = 0
            for (var i = 0; i < tabs.panelList.length; i++)
                if (tabs.panelList[i].detached) n++
            return n
        }
        readonly property int detachedBarH: detachedCount > 0 ? barH : 0
        onLoaded: {
            item.groupIdx = -1; item.height = barH
            item.tabIndices = Qt.binding(function () {
                var r = []
                for (var i = 0; i < tabs.panelList.length; i++)
                    if (tabs.panelList[i].detached) r.push(i)
                return r
            })
        }
    }

    // ── content area — single SplitView, N children, native resize handles
    SplitView {
        id: contentSplit
        anchors { left: parent.left; right: parent.right; top: detachedLoader.bottom; bottom: parent.bottom }
        orientation: Qt.Horizontal
        // SplitView handles are draggable by default — they appear between
        // each pair of child items and are visible as thin bars.
    }

    // ══════════════════════════════════════════════════════════════════════
    // Drag overlays
    // ══════════════════════════════════════════════════════════════════════

    Rectangle {
        visible: tabs.dragging && tabs.dropZone === "left-edge"
        z: 9998; x: 0; y: barH + (detachedLoader.detachedBarH || 0)
        width: parent.width / 2
        height: parent.height - barH - (detachedLoader.detachedBarH || 0)
        color: "#182196f3"; border.color: "#2196f3"; border.width: 2
        Label {
            anchors.centerIn: parent
            text: dragTitle(); color: "#90caf9"; font.pixelSize: 20; font.bold: true
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }
    Rectangle {
        visible: tabs.dragging && tabs.dropZone === "right-edge"
        z: 9998; x: parent.width / 2; y: barH + (detachedLoader.detachedBarH || 0)
        width: parent.width / 2
        height: parent.height - barH - (detachedLoader.detachedBarH || 0)
        color: "#182196f3"; border.color: "#2196f3"; border.width: 2
        Label {
            anchors.centerIn: parent
            text: dragTitle(); color: "#90caf9"; font.pixelSize: 20; font.bold: true
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }
    Rectangle {
        visible: tabs.dragging && tabs.dropZone === "split-left" && tabs.dropTargetGroup >= 0
        z: 9998
        x: dropColX(); y: barH + (detachedLoader.detachedBarH || 0)
        width: dropColW() * tabs.edgeFrac
        height: parent.height - barH - (detachedLoader.detachedBarH || 0)
        color: "#182196f3"; border.color: "#2196f3"; border.width: 2
        Label {
            anchors.centerIn: parent
            text: dragTitle(); color: "#90caf9"; font.pixelSize: 18; font.bold: true
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }
    Rectangle {
        visible: tabs.dragging && tabs.dropZone === "split-right" && tabs.dropTargetGroup >= 0
        z: 9998
        x: dropColX() + dropColW() * (1 - tabs.edgeFrac); y: barH + (detachedLoader.detachedBarH || 0)
        width: dropColW() * tabs.edgeFrac
        height: parent.height - barH - (detachedLoader.detachedBarH || 0)
        color: "#182196f3"; border.color: "#2196f3"; border.width: 2
        Label {
            anchors.centerIn: parent
            text: dragTitle(); color: "#90caf9"; font.pixelSize: 18; font.bold: true
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }
    Rectangle {
        visible: tabs.dragging && !tabs.isSplit && tabs.dropZone === "content"
        z: 9998; x: 0; y: barH + (detachedLoader.detachedBarH || 0)
        width: parent.width
        height: parent.height - barH - (detachedLoader.detachedBarH || 0)
        color: "transparent"; border.color: "#2196f3"; border.width: 3
        Rectangle { anchors.fill: parent; anchors.margins: 3; color: "#0c2196f3" }
        Label {
            anchors.centerIn: parent
            text: dragTitle(); color: "#90caf9"; font.pixelSize: 22; font.bold: true
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }
    Rectangle {
        visible: tabs.dragging && tabs.isSplit && tabs.dropZone === "content"
                 && tabs.dropTargetGroup >= 0 && tabs.dropTargetGroup < tabs.groups.length
        z: 9998
        x: dropColX(); y: barH + (detachedLoader.detachedBarH || 0)
        width: dropColW()
        height: parent.height - barH - (detachedLoader.detachedBarH || 0)
        color: "#182196f3"; border.color: "#2196f3"; border.width: 2
        Label {
            anchors.centerIn: parent
            text: dragTitle(); color: "#90caf9"; font.pixelSize: 20; font.bold: true
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }

    function dragTitle() {
        return dragPanelIdx >= 0 && dragPanelIdx < panelList.length ? panelList[dragPanelIdx].title : ""
    }
    function dropColX() {
        var g = dropTargetGroup
        if (g < 0 || g >= colItems.length) return parent.width / 2
        var c = colItems[g]; return c ? c.x : parent.width / 2
    }
    function dropColW() {
        var g = dropTargetGroup
        if (g < 0 || g >= colItems.length) return parent.width / 2
        var c = colItems[g]; return c ? c.width : parent.width / 2
    }

    // Tab-bar insertion line
    Rectangle {
        visible: tabs.dragging && tabs.dropTargetGroup >= 0 && tabs.dropZone === "bar"
        z: 9999; x: barInsertLineGlobalX(); y: barInsertLineY()
        width: 2; height: barH; color: "#2196f3"
    }
    function barInsertLineY() { return detachedLoader.detachedBarH || 0 }
    function barInsertLineGlobalX() {
        var g = dropTargetGroup
        if (g < 0 || g >= groups.length) return 0
        var loader = barLoaders[g]
        if (!loader || !loader.item) return 0
        var item = loader.item
        var kids = item.visibleChildren, nth = 0, localX = 0
        for (var i = 0; i < kids.length; i++) {
            var c = kids[i]
            if (!c || c._isOverlay) continue
            if (nth === dropBarIdx) { localX = c.x - 1; break }; nth++
        }
        if (localX === 0) {
            for (var j = kids.length - 1; j >= 0; j--) {
                var d = kids[j]
                if (!d || d._isOverlay) continue
                localX = d.x + d.width + 1; break
            }
        }
        var gp = item.mapToItem(tabs, localX, 0)
        return gp.x
    }

    // drag ghost
    DragGhost {
        visible: tabs.dragging
        label: dragTitle()
        cx: tabs.dragCurX; cy: tabs.dragCurY
    }
}
