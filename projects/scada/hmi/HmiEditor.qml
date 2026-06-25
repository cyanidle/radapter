import QtQuick 2.7
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import ".."   // DockHost / DockablePanel live one dir up in projects/scada/
// Node, Gauge, InfoDisplay are sibling .qml files (same-directory resolution).

// Visualization (HMI) editor, embedded as a tab in the configurator. Edits a tree of
// layout containers (Row/Column/Grid) and widget leaves (Gauge/InfoDisplay). Three panes:
//   - structure tree: the node hierarchy; add/remove/reorder nodes
//   - WYSIWYG canvas: the real layout rendered via Node { mode: "design" }
//   - property panel: edit the selected node's props + layout hints
// Every edit commits back to the shared ConfigContext, so the configurator's preview and
// Run payload always carry the current visualization.
Item {
    id: editor

    property var context: null          // shared ConfigContext

    // live overlay fed by the configurator while a runner streams tags (see Configurator.qml)
    property var liveValues: undefined
    property var liveQuality: undefined
    property bool live: false

    // candidate "worker:field" tags, derived live from the authored config (re-evaluated
    // on every context edit via revision), sorted so they cluster by worker-name prefix.
    property var candidateTags: context ? deriveTags(context.objects, context.revision) : []

    property var viz: ({ root: { type: "Column", spacing: 8, children: [] } })
    property var selectedPath: null     // array of child indices; [] is the root
    property var rows: []               // flattened tree rows for the structure list
    property var fields: []             // property descriptors for the selected node
    property var clipboard: null        // deep-cloned node for Ctrl+C / Ctrl+V duplication

    readonly property var containerTypes: ["Row", "Column", "Grid"]
    readonly property var widgetTypes: ["Gauge", "InfoDisplay", "Chart", "Spacer", "Custom"]

    // Tags follow the "<worker>:<field>" convention (see src/tags.cpp). Walk each
    // Modbus object's register tables for the field names. Best-effort — the tag field
    // is also free-text, so anything not discovered here can still be typed in.
    function deriveTags(objects, _rev) {
        var out = []
        for (var name in (objects || ({}))) {
            var o = objects[name]
            if (!o || (o.type !== "ModbusMaster" && o.type !== "ModbusSlave")) continue
            var regs = (o.config || ({})).registers || ({})
            for (var group in regs) {
                var g = regs[group]
                if (g && typeof g === "object")
                    for (var field in g) out.push(name + ":" + field)
            }
        }
        out.sort()
        return out
    }

    function isContainerType(t) { return containerTypes.indexOf(t) >= 0 }
    function clone(o) { return JSON.parse(JSON.stringify(o)) }

    // ---- tree navigation ----------------------------------------------------
    function nodeAt(path) {
        var n = viz.root
        for (var i = 0; i < path.length; i++) n = n.children[path[i]]
        return n
    }
    function pathEq(a, b) {
        if (a === null || b === null) return false
        return JSON.stringify(a) === JSON.stringify(b)
    }

    // ---- load / commit ------------------------------------------------------
    function loadFromContext() {
        if (!context) return
        var v = context.visualization
        viz = (v && v.root) ? clone(v) : { root: { type: "Column", spacing: 8, children: [] } }
        selectedPath = []
        refresh()
    }
    function refresh() {                       // full rebuild: tree rows + property fields
        rows = buildRows()
        fields = fieldsFor(selectedPath ? nodeAt(selectedPath) : null)
    }
    // Selection must NOT rebuild the tree: reassigning `rows` resets the ListView and
    // destroys every delegate — including the one whose MouseArea is mid-click — which
    // crashes on fast clicking. The selected highlight reacts to `selectedPath` via
    // bindings, so selecting only needs to refresh the property panel.
    function selectNode(path) {
        selectedPath = path
        fields = fieldsFor(path ? nodeAt(path) : null)
        if (path) radapter.note("hmi:node_selected|" + nodeAt(path).type + "|" + path.join(","))
    }
    function deselectAll() {
        selectedPath = null
        _lastClickPath = null
        fields = []
        radapter.note("hmi:node_deselected")
    }

    // Canvas click selection cycles through the stack under the cursor: the first click on a
    // spot selects the deepest (lowest) node there; clicking the same spot again walks the
    // selection up the ancestor chain to the root, then wraps back to the deepest. The
    // deepest node always receives the click (child MouseAreas sit above their container's),
    // so `deepestPath` is what it reports.
    property var _lastClickPath: null
    function cycleSelect(deepestPath) {
        var chain = []
        var p = deepestPath.slice()
        while (true) { chain.push(p.slice()); if (p.length === 0) break; p = p.slice(0, -1) }
        if (pathEq(deepestPath, _lastClickPath)) {
            var idx = 0
            for (var i = 0; i < chain.length; i++) if (pathEq(chain[i], selectedPath)) { idx = i; break }
            selectNode(chain[(idx + 1) % chain.length])
        } else {
            selectNode(deepestPath)
        }
        _lastClickPath = deepestPath.slice()
    }

    // Structural edits defer the heavy rebuild (canvas re-clone + tree rebuild) to the next
    // event-loop turn via Qt.callLater, so it never runs re-entrantly inside an input/click
    // handler (which is the other way fast clicking could destroy an item under the grab).
    // The flag coalesces a burst of edits into a single rebuild.
    property bool _commitPending: false
    function commit() {
        _commitPending = true
        Qt.callLater(flushCommit)
    }
    function flushCommit() {
        if (!_commitPending) return
        _commitPending = false
        viz = clone(viz)                     // new reference so the canvas Node re-renders
        if (context) context.setVisualization(clone(viz))
        refresh()
    }

    function buildRows() {
        var out = []
        function walk(node, path, depth) {
            var name = path.length === 0 ? "Root" : node.type
            out.push({ path: path, depth: depth,
                       label: name + (node.tag ? "  ⟨" + node.tag + "⟩" : "") })
            if (node.children)
                for (var i = 0; i < node.children.length; i++)
                    walk(node.children[i], path.concat([i]), depth + 1)
        }
        walk(viz.root, [], 0)
        return out
    }

    // ---- structural edits ---------------------------------------------------
    function makeNode(type) {
        if (type === "Grid")  return { type: type, spacing: 8, columns: 2, children: [] }
        if (isContainerType(type)) return { type: type, spacing: 8, children: [] }
        if (type === "Gauge") return { type: type, tag: "", min: 0, max: 100, label: type, units: "" }
        if (type === "Chart") return { type: type, tag: "", timeFrame: 3600, label: type, units: "" }
        if (type === "Spacer") return { type: type, fillWidth: true }
        if (type === "Custom") return { type: type, source: "", tag: "" }
        return { type: type, tag: "", label: type, units: "" }   // InfoDisplay & others
    }
    function addNode(type) {
        // add as a child of the selected container, else the selected node's parent,
        // else the root
        var target = viz.root
        if (selectedPath && selectedPath.length >= 0) {
            var sel = nodeAt(selectedPath)
            if (isContainerType(sel.type)) target = sel
            else if (selectedPath.length > 0) target = nodeAt(selectedPath.slice(0, -1))
        }
        if (!target.children) target.children = []
        target.children.push(makeNode(type))
        radapter.note("hmi:node_added|" + type)
        commit()
    }
    // add a child of a specific container (the in-canvas "＋" button, by node path)
    function addChild(containerPath, type) {
        var target = nodeAt(containerPath)
        if (!isContainerType(target.type)) return
        if (!target.children) target.children = []
        target.children.push(makeNode(type))
        selectedPath = containerPath.concat([target.children.length - 1])
        radapter.note("hmi:child_added|" + type + "|" + containerPath.join(","))
        commit()
    }

    // ---- clipboard (copy / cut / paste a subtree) ----------------------------
    function copySelected() {
        if (selectedPath) { clipboard = clone(nodeAt(selectedPath)); radapter.note("hmi:node_copied") }
    }
    function cutSelected() {
        if (!selectedPath || selectedPath.length === 0) return   // never cut the root
        clipboard = clone(nodeAt(selectedPath))
        radapter.note("hmi:node_cut")
        removeSelected()
    }
    // Paste the clipboard node. If the selection is a container, paste into it as a new
    // child; otherwise paste as the sibling right after the selection. With nothing selected
    // (or the root), append into the root.
    function pasteClipboard() {
        if (!clipboard) return
        var node = clone(clipboard)
        if (selectedPath && selectedPath.length > 0) {
            var sel = nodeAt(selectedPath)
            if (isContainerType(sel.type)) {
                if (!sel.children) sel.children = []
                sel.children.push(node)
                selectedPath = selectedPath.concat([sel.children.length - 1])
            } else {
                var parent = nodeAt(selectedPath.slice(0, -1))
                var at = selectedPath[selectedPath.length - 1] + 1
                parent.children.splice(at, 0, node)
                selectedPath = selectedPath.slice(0, -1).concat([at])
            }
        } else {
            if (!viz.root.children) viz.root.children = []
            viz.root.children.push(node)
            selectedPath = [viz.root.children.length - 1]
        }
        radapter.note("hmi:node_pasted")
        commit()
    }
    function removeSelected() {
        if (!selectedPath || selectedPath.length === 0) return   // never remove the root
        var node = nodeAt(selectedPath)
        var parent = nodeAt(selectedPath.slice(0, -1))
        parent.children.splice(selectedPath[selectedPath.length - 1], 1)
        radapter.note("hmi:node_removed|" + node.type + "|" + selectedPath.join(","))
        selectedPath = null
        commit()
    }
    // is `a` a prefix of (or equal to) `b`? — used to forbid dropping a node into its
    // own subtree, which would detach the branch from the tree.
    function isPrefix(a, b) {
        if (a.length > b.length) return false
        for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false
        return true
    }

    // Move the node at fromPath to be child #toIndex of the node at toParentPath.
    function moveNode(fromPath, toParentPath, toIndex) {
        if (!fromPath || fromPath.length === 0) return        // never move the root
        if (isPrefix(fromPath, toParentPath)) return          // not into own subtree
        var node = nodeAt(fromPath)
        var fromParent = nodeAt(fromPath.slice(0, -1))
        var fromIdx = fromPath[fromPath.length - 1]
        var sameParent = pathEq(fromPath.slice(0, -1), toParentPath)
        var insertIdx = (sameParent && fromIdx < toIndex) ? toIndex - 1 : toIndex
        fromParent.children.splice(fromIdx, 1)
        var toParent = nodeAt(toParentPath)
        if (!toParent.children) toParent.children = []
        toParent.children.splice(insertIdx, 0, node)
        selectedPath = toParentPath.concat([insertIdx])
        commit()
    }

    // Resolve a drag-drop onto a target row into a moveNode call. zone: -1 before the
    // target, +1 after it, 0 "into" it (only when the target is a container).
    function dropOnto(fromPath, targetPath, zone) {
        var toParentPath, toIndex
        if (zone === 0) {
            var t = nodeAt(targetPath)
            if (isContainerType(t.type)) {
                toParentPath = targetPath
                toIndex = t.children ? t.children.length : 0
                moveNode(fromPath, toParentPath, toIndex)
                radapter.note("hmi:node_dropped_into|" + targetPath.join(","))
                return
            }
            zone = 1   // a leaf can't contain — fall back to "after"
        }
        if (targetPath.length === 0) {   // before/after the root → append into the root
            moveNode(fromPath, [], nodeAt([]).children ? nodeAt([]).children.length : 0)
            return
        }
        toParentPath = targetPath.slice(0, -1)
        var idx = targetPath[targetPath.length - 1]
        moveNode(fromPath, toParentPath, zone < 0 ? idx : idx + 1)
        radapter.note("hmi:node_dropped|" + targetPath.join(",") + "|" + (zone < 0 ? "before" : "after"))
    }

    function moveSelected(delta) {
        if (!selectedPath || selectedPath.length === 0) return
        var parent = nodeAt(selectedPath.slice(0, -1))
        var idx = selectedPath[selectedPath.length - 1]
        var j = idx + delta
        if (j < 0 || j >= parent.children.length) return
        var c = parent.children
        var tmp = c[idx]; c[idx] = c[j]; c[j] = tmp
        selectedPath = selectedPath.slice(0, -1).concat([j])
        radapter.note("hmi:node_moved|" + selectedPath.join(",") + "|" + (delta > 0 ? "down" : "up"))
        commit()
    }
    function setProp(key, value) {
        if (!selectedPath) return
        var n = nodeAt(selectedPath)
        if (value === "" || value === undefined || value === null) delete n[key]
        else n[key] = value
        radapter.note("hmi:prop_changed|" + n.type + "|" + key)
        commit()
    }

    // ---- property descriptors for the selected node -------------------------
    function fieldsFor(node) {
        if (!node) return []
        var f = []
        if (isContainerType(node.type)) {
            f.push({ key: "spacing", label: "Spacing", kind: "number" })
            if (node.type === "Grid") f.push({ key: "columns", label: "Columns", kind: "number" })
        } else if (node.type === "Spacer") {
            // a spacer carries only layout hints (appended below)
        } else if (node.type === "Custom") {
            f.push({ key: "source", label: "QML source", kind: "text" })
            f.push({ key: "tag", label: "Tag", kind: "tag" })
        } else {
            f.push({ key: "tag", label: "Tag", kind: "tag" })
            f.push({ key: "label", label: "Label", kind: "text" })
            f.push({ key: "units", label: "Units", kind: "text" })
            if (node.type === "Gauge") {
                f.push({ key: "min", label: "Min", kind: "number" })
                f.push({ key: "max", label: "Max", kind: "number" })
                f.push({ key: "color", label: "Arc color", kind: "text" })
            }
            if (node.type === "InfoDisplay")
                f.push({ key: "decimals", label: "Decimals", kind: "number" })
            if (node.type === "Chart") {
                f.push({ key: "timeFrame", label: "Time frame (s)", kind: "number" })
                f.push({ key: "yMin", label: "Y min", kind: "number" })
                f.push({ key: "yMax", label: "Y max", kind: "number" })
                f.push({ key: "color", label: "Line color", kind: "text" })
            }
        }
        f.push({ key: "fillWidth", label: "Fill width", kind: "bool" })
        f.push({ key: "fillHeight", label: "Fill height", kind: "bool" })
        f.push({ key: "preferredWidth", label: "Pref. width", kind: "number" })
        f.push({ key: "preferredHeight", label: "Pref. height", kind: "number" })
        return f
    }

    onContextChanged: loadFromContext()
    onVisibleChanged: if (visible) loadFromContext()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

    // ── toolbar: add nodes / remove / reorder ────────────────────────────────
    ToolBar {
        Layout.fillWidth: true
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6

            // only layout containers live on the toolbar; widgets are added in-place via
            // the "＋" button inside a container (see Node.qml), or pasted into the tree.
            Label { text: "Add layout:"; font.bold: true }
            Repeater {
                model: editor.containerTypes
                delegate: Button {
                    text: modelData
                    onClicked: editor.addNode(modelData)
                }
            }
            ToolSeparator {}
            Button {
                text: "Remove"
                enabled: editor.selectedPath && editor.selectedPath.length > 0
                onClicked: editor.removeSelected()
            }
            Button { text: "▲"; enabled: editor.selectedPath && editor.selectedPath.length > 0
                     onClicked: editor.moveSelected(-1) }
            Button { text: "▼"; enabled: editor.selectedPath && editor.selectedPath.length > 0
                     onClicked: editor.moveSelected(1) }
            Item { Layout.fillWidth: true }
            Row {
                visible: editor.live
                spacing: 4
                Rectangle { width: 10; height: 10; radius: 5; color: "#43a047"
                            anchors.verticalCenter: parent.verticalCenter }
                Label { text: "LIVE"; color: "#43a047"; font.bold: true
                        anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }

    DockHost {
        id: hmiDock
        Layout.fillWidth: true
        Layout.fillHeight: true
        appWindow: Window.window
        centerContent: hmiCenter

        // structure tree + canvas in the center; the property inspector docks to an edge
        SplitView {
            id: hmiCenter
            anchors.fill: parent ? parent : undefined
            orientation: Qt.Horizontal

        // structure tree
        ColumnLayout {
            SplitView.preferredWidth: 220
            SplitView.minimumWidth: 160
            Label { text: "Structure"; font.bold: true; padding: 6 }
            ListView {
                id: treeList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                focus: true
                model: editor.rows
                // Delete key removes the selected node (no-op on the root). Scoped to the
                // tree's focus so it doesn't hijack Delete while editing a property field.
                Keys.onPressed: {
                    if (event.key === Qt.Key_Escape) {
                        editor.deselectAll()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                        editor.removeSelected()
                        event.accepted = true
                    } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C) {
                        editor.copySelected()
                        event.accepted = true
                    } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_X) {
                        editor.cutSelected()
                        event.accepted = true
                    } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_V) {
                        editor.pasteClipboard()
                        event.accepted = true
                    }
                }
                // Each row is draggable (reorder/reparent) and a drop target. A row is
                // dropped before/after a sibling, or "into" a container node.
                delegate: Item {
                    id: wrapper
                    width: treeList.width
                    height: 26
                    property var rowData: modelData
                    readonly property bool selected: editor.pathEq(rowData.path, editor.selectedPath)

                    Rectangle {
                        id: content
                        width: wrapper.width
                        height: wrapper.height
                        color: dragArea.drag.active ? "#bbdefb"
                             : wrapper.selected     ? "#e3f2fd" : "transparent"
                        Drag.active: dragArea.drag.active
                        Drag.source: wrapper
                        Drag.hotSpot.x: 12
                        Drag.hotSpot.y: 13

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            x: 8 + wrapper.rowData.depth * 16
                            width: parent.width - x - 4
                            text: wrapper.rowData.label
                            elide: Text.ElideRight
                            color: wrapper.selected ? "#1565c0" : "#333"
                        }

                        // while dragging, float above the list so the row follows the cursor
                        states: State {
                            when: dragArea.drag.active
                            ParentChange { target: content; parent: treeList }
                            AnchorChanges { target: content; anchors.verticalCenter: undefined }
                        }
                    }

                    MouseArea {
                        id: dragArea
                        anchors.fill: parent
                        drag.target: content
                        drag.axis: Drag.YAxis
                        onClicked: {
                            editor.selectNode(wrapper.rowData.path)
                            treeList.forceActiveFocus()
                        }
                        onReleased: {
                            if (drag.active) content.Drag.drop()
                            content.x = 0; content.y = 0   // snap back into the list slot
                        }
                    }

                    // drop indicator: a line at the top/bottom for before/after, or a full
                    // outline when dropping into a container.
                    DropArea {
                        id: drop
                        anchors.fill: parent
                        property int zone: 0   // -1 before, 0 into, +1 after
                        property bool active: false
                        onEntered: active = true
                        onExited: active = false
                        onDropped: {
                            active = false
                            editor.dropOnto(drag.source.rowData.path, wrapper.rowData.path, zone)
                        }
                        onPositionChanged: {
                            var f = drag.y / wrapper.height
                            zone = f < 0.3 ? -1 : (f > 0.7 ? 1 : 0)
                        }

                        Rectangle {   // into-container highlight
                            anchors.fill: parent
                            color: "transparent"
                            border.color: "#1565c0"; border.width: 2
                            visible: drop.active && drop.zone === 0
                                     && editor.isContainerType(wrapper.rowData.type)
                        }
                        Rectangle {   // before/after insertion line
                            height: 2; color: "#1565c0"
                            anchors.left: parent.left; anchors.right: parent.right
                            y: drop.zone < 0 ? 0 : parent.height - 2
                            visible: drop.active &&
                                     (drop.zone !== 0 || !editor.isContainerType(wrapper.rowData.type))
                        }
                    }
                }
            }
        }

        // WYSIWYG canvas
        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 240
            Label { text: "Layout (click a widget to edit)"; font.bold: true; padding: 6 }
            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Flickable {
                    id: canvasFlick
                    anchors.fill: parent
                    contentWidth: width                       // wrap to viewport width (Node wraps overflow)
                    contentHeight: canvas.implicitHeight
                    clip: true
                    // scrollable by dragging the bars and by keyboard (not just the wheel)
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                    activeFocusOnTab: true
                    function maxY() { return Math.max(0, contentHeight - height) }
                    Keys.onPressed: {
                        var step = 40
                        if (event.key === Qt.Key_Down)      contentY = Math.min(contentY + step, maxY())
                        else if (event.key === Qt.Key_Up)   contentY = Math.max(contentY - step, 0)
                        else if (event.key === Qt.Key_PageDown) contentY = Math.min(contentY + height * 0.9, maxY())
                        else if (event.key === Qt.Key_PageUp)   contentY = Math.max(contentY - height * 0.9, 0)
                        else if (event.key === Qt.Key_Home) contentY = 0
                        else if (event.key === Qt.Key_End)  contentY = maxY()
                        else return
                        event.accepted = true
                    }
                    Node {
                        id: canvas
                        width: parent.width
                        spec: editor.viz.root
                        mode: "design"
                        path: []
                        selectedPath: editor.selectedPath
                        liveValues: editor.liveValues
                        liveQuality: editor.liveQuality
                        addTypes: editor.containerTypes.concat(editor.widgetTypes)
                        onSelectRequested: {
                            editor.cycleSelect(path)
                            treeList.forceActiveFocus()
                        }
                        onAddRequested: editor.addChild(path, type)
                    }
                }
            }
        }

        }   // hmiCenter SplitView

        // property inspector — dockable to an edge, or floatable into its own window
        DockablePanel {
            id: hmiProps
            side: "right"; homeSide: "right"
            title: !editor.selectedPath ? "Properties"
                 : editor.selectedPath.length === 0 ? "Properties — Root"
                 : ("Properties — " + editor.nodeAt(editor.selectedPath).type)
            ColumnLayout {
                anchors.fill: parent ? parent : undefined
            ScrollView {
                id: propScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 8   // breathing room so fields don't butt the panel edge
                clip: true
                ColumnLayout {
                    width: propScroll.availableWidth   // fit the panel, not the whole window
                    spacing: 8

                    Repeater {
                        model: editor.fields
                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            Layout.rightMargin: 8
                            spacing: 2
                            property var field: modelData
                            property var node: editor.selectedPath ? editor.nodeAt(editor.selectedPath) : ({})

                            Label { text: field.label; font.pixelSize: 11; color: "#666" }

                            // Each control mirrors node[field.key] through a `boundValue`
                            // that re-evaluates when the selection changes, and pushes it to
                            // the display unless the control is being actively edited. This
                            // is what keeps the panel correct on selection change: a plain
                            // binding breaks once the user types, and a one-time
                            // Component.onCompleted (the old tag combo) never refreshes at all.

                            // text
                            Loader {
                                Layout.fillWidth: true
                                active: field.kind === "text"
                                sourceComponent: TextField {
                                    property string boundValue: node[field.key] !== undefined ? String(node[field.key]) : ""
                                    text: boundValue
                                    onBoundValueChanged: if (!activeFocus) text = boundValue
                                    onEditingFinished: editor.setProp(field.key, text)
                                }
                            }
                            // number
                            Loader {
                                Layout.fillWidth: true
                                active: field.kind === "number"
                                sourceComponent: TextField {
                                    property string boundValue: node[field.key] !== undefined ? String(node[field.key]) : ""
                                    text: boundValue
                                    onBoundValueChanged: if (!activeFocus) text = boundValue
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    validator: DoubleValidator {}
                                    onEditingFinished:
                                        editor.setProp(field.key, text === "" ? "" : Number(text))
                                }
                            }
                            // bool
                            Loader {
                                active: field.kind === "bool"
                                sourceComponent: CheckBox {
                                    property bool boundValue: node[field.key] === true
                                    checked: boundValue
                                    onBoundValueChanged: if (!pressed) checked = boundValue
                                    onToggled: editor.setProp(field.key, checked ? true : "")
                                }
                            }
                            // tag combo (editable; model clusters by worker prefix)
                            Loader {
                                Layout.fillWidth: true
                                active: field.kind === "tag"
                                sourceComponent: ComboBox {
                                    editable: true
                                    model: editor.candidateTags
                                    property string boundValue: node[field.key] !== undefined ? String(node[field.key]) : ""
                                    onBoundValueChanged: if (!activeFocus) editText = boundValue
                                    Component.onCompleted: editText = boundValue
                                    onAccepted: editor.setProp(field.key, editText)
                                    onActivated: if (currentIndex >= 0) editor.setProp(field.key, currentText)
                                }
                            }
                        }
                    }
                }
            }
        }
        }   // DockablePanel hmiProps
    }       // DockHost
    }
}
