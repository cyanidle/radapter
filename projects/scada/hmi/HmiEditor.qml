import QtQuick 2.7
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
// Node, Gauge, InfoDisplay are sibling .qml files (same-directory resolution).

// Visualization (HMI) editor window, opened from the configurator. Edits a tree of layout
// containers (Row/Column/Grid) and widget leaves (Gauge/InfoDisplay). Three panes:
//   - structure tree: the node hierarchy; add/remove/reorder nodes
//   - WYSIWYG canvas: the real layout rendered via Node { mode: "design" }
//   - property panel: edit the selected node's props + layout hints
// Every edit commits back to the shared ConfigContext, so the configurator's preview and
// Run payload always carry the current visualization.
ApplicationWindow {
    id: editor
    visible: false
    width: 880
    height: 620
    title: "Visualization Editor"

    property var context: null          // shared ConfigContext
    property var candidateTags: []      // "worker:field" strings, grouped by worker prefix

    property var viz: ({ root: { type: "Column", spacing: 8, children: [] } })
    property var selectedPath: null     // array of child indices; [] is the root
    property var rows: []               // flattened tree rows for the structure list
    property var fields: []             // property descriptors for the selected node

    readonly property var containerTypes: ["Row", "Column", "Grid"]
    readonly property var widgetTypes: ["Gauge", "InfoDisplay"]

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
    function refresh() {
        rows = buildRows()
        fields = fieldsFor(selectedPath ? nodeAt(selectedPath) : null)
    }
    function commit() {
        viz = clone(viz)                     // new reference so the canvas Node re-renders
        if (context) context.setVisualization(clone(viz))
        refresh()
    }

    function buildRows() {
        var out = []
        function walk(node, path, depth) {
            out.push({ path: path, depth: depth,
                       label: node.type + (node.tag ? "  ⟨" + node.tag + "⟩" : "") })
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
        commit()
    }
    function removeSelected() {
        if (!selectedPath || selectedPath.length === 0) return   // never remove the root
        var parent = nodeAt(selectedPath.slice(0, -1))
        parent.children.splice(selectedPath[selectedPath.length - 1], 1)
        selectedPath = null
        commit()
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
        commit()
    }
    function setProp(key, value) {
        if (!selectedPath) return
        var n = nodeAt(selectedPath)
        if (value === "" || value === undefined || value === null) delete n[key]
        else n[key] = value
        commit()
    }

    // ---- property descriptors for the selected node -------------------------
    function fieldsFor(node) {
        if (!node) return []
        var f = []
        if (isContainerType(node.type)) {
            f.push({ key: "spacing", label: "Spacing", kind: "number" })
            if (node.type === "Grid") f.push({ key: "columns", label: "Columns", kind: "number" })
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
        }
        f.push({ key: "fillWidth", label: "Fill width", kind: "bool" })
        f.push({ key: "fillHeight", label: "Fill height", kind: "bool" })
        f.push({ key: "preferredWidth", label: "Pref. width", kind: "number" })
        f.push({ key: "preferredHeight", label: "Pref. height", kind: "number" })
        return f
    }

    onContextChanged: loadFromContext()
    onVisibleChanged: if (visible) loadFromContext()

    // ── toolbar: add nodes / remove / reorder ────────────────────────────────
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6

            Label { text: "Add:"; font.bold: true }
            Repeater {
                model: editor.containerTypes.concat(editor.widgetTypes)
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
        }
    }

    SplitView {
        anchors.fill: parent
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
                model: editor.rows
                delegate: ItemDelegate {
                    width: treeList.width
                    height: 26
                    highlighted: editor.pathEq(modelData.path, editor.selectedPath)
                    onClicked: { editor.selectedPath = modelData.path; editor.refresh() }
                    contentItem: Text {
                        text: modelData.label
                        leftPadding: 8 + modelData.depth * 16
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        color: parent.highlighted ? "#1565c0" : "#333"
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
                    anchors.fill: parent
                    contentWidth: width
                    contentHeight: canvas.implicitHeight
                    clip: true
                    Node {
                        id: canvas
                        width: parent.width
                        spec: editor.viz.root
                        mode: "design"
                        path: []
                        selectedPath: editor.selectedPath
                        onSelectRequested: { editor.selectedPath = path; editor.refresh() }
                    }
                }
            }
        }

        // property panel
        ColumnLayout {
            SplitView.preferredWidth: 260
            SplitView.minimumWidth: 200
            Label {
                text: editor.selectedPath ? ("Properties — " + editor.nodeAt(editor.selectedPath).type)
                                          : "Select a node"
                font.bold: true
                padding: 6
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ColumnLayout {
                    width: editor.width  // bounded by ScrollView; fields stretch
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

                            // text
                            Loader {
                                Layout.fillWidth: true
                                active: field.kind === "text"
                                sourceComponent: TextField {
                                    text: node[field.key] !== undefined ? String(node[field.key]) : ""
                                    onEditingFinished: editor.setProp(field.key, text)
                                }
                            }
                            // number
                            Loader {
                                Layout.fillWidth: true
                                active: field.kind === "number"
                                sourceComponent: TextField {
                                    text: node[field.key] !== undefined ? String(node[field.key]) : ""
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
                                    checked: node[field.key] === true
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
                                    Component.onCompleted:
                                        editText = node[field.key] !== undefined ? String(node[field.key]) : ""
                                    onAccepted: editor.setProp(field.key, editText)
                                    onActivated: editor.setProp(field.key, editText)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
