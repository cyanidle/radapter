import QtQuick 2.7
import QtQuick.Layouts 1.3

// Recursive renderer for one visualization node. A node is either a layout CONTAINER
// (type Row/Column/Grid, with `children`) or a widget LEAF (type Gauge/InfoDisplay/…,
// with a `tag`). The same component renders the runtime HMI (`mode: "run"`, value bound
// to radapter.tags[spec.tag]) and the editor canvas (`mode: "design"`, placeholder value
// + click-to-select). Selection is by `path` (array of child indices) so it survives the
// editor's deep-copy-on-edit.
Loader {
    id: node

    property var spec: ({})
    property string mode: "run"            // "run" | "design"
    property var path: []                  // index path from the root
    property var selectedPath: null        // editor's current selection (design mode)
    signal selectRequested(var path)

    function isContainer(t) { return t === "Row" || t === "Column" || t === "Grid" }
    function pathEq(a, b) {
        if (a === null || b === null) return false
        if (a.length !== b.length) return false
        for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false
        return true
    }

    // layout hints — applied to this Loader as seen by its parent layout
    Layout.fillWidth: spec.fillWidth === true
    Layout.fillHeight: spec.fillHeight === true
    Layout.preferredWidth: spec.preferredWidth !== undefined ? spec.preferredWidth : implicitWidth
    Layout.preferredHeight: spec.preferredHeight !== undefined ? spec.preferredHeight : implicitHeight
    Layout.minimumWidth: implicitWidth > 0 ? Math.min(implicitWidth, 40) : 0

    // resolved value/quality for leaf widgets. Only touch radapter.tags in run mode
    // (the editor process has no --tags, so radapter.tags is null there).
    readonly property var resolvedValue:
        mode === "run" && spec.tag ? radapter.tags[spec.tag]
                                   : (mode === "design" ? designValue() : undefined)
    readonly property string resolvedQuality:
        mode === "run" && spec.tag ? (radapter.quality[spec.tag] || "comm_fail") : "good"

    // a representative placeholder so widgets look alive in the editor
    function designValue() {
        if (spec.min !== undefined && spec.max !== undefined)
            return (spec.min + spec.max) / 2
        return 42
    }

    Component.onCompleted: {
        if (mode === "run" && spec.tag && radapter.tags) radapter.tags.ensure(spec.tag)
    }

    sourceComponent: isContainer(spec.type) ? containerComp : leafComp

    // ---- container: a *Layout with a nested Node per child ------------------
    Component {
        id: containerComp
        Item {
            implicitWidth: lay.implicitWidth
            implicitHeight: lay.implicitHeight

            // background click-catcher, BEHIND the children: clicking empty container
            // space selects this container, but a click on a child widget is grabbed by
            // the child's own MouseArea on top (so selection follows what you clicked).
            // Layering is by explicit non-negative z, never negative: a negative z pushes
            // this area behind the parent and into the GRANDPARENT's stacking context,
            // where a nested container's background leaks in front of an earlier sibling's
            // leaf widgets — which made clicks on the first row select the row, not the item.
            MouseArea {
                z: 0
                anchors.fill: parent
                enabled: node.mode === "design"
                onClicked: node.selectRequested(node.path)
            }

            GridLayout {
                id: lay
                z: 1
                anchors.fill: parent
                rowSpacing: node.spec.spacing !== undefined ? node.spec.spacing : 6
                columnSpacing: node.spec.spacing !== undefined ? node.spec.spacing : 6
                // Row -> one row, Column -> one column, Grid -> spec.columns wide
                flow: node.spec.type === "Row" ? GridLayout.LeftToRight : GridLayout.TopToBottom
                columns: node.spec.type === "Grid"
                    ? (node.spec.columns !== undefined ? node.spec.columns : 2)
                    : (node.spec.type === "Row" ? 1000000 : 1)
                rows: node.spec.type === "Column" ? 1000000 : 1

                Repeater {
                    model: node.spec.children ? node.spec.children.length : 0
                    // child Nodes are loaded by URL (not `Node {}`) so QML allows the
                    // recursion; the Loader is the layout's direct child, so it carries
                    // the child's Layout.* hints.
                    delegate: Loader {
                        id: childLoader
                        property var childSpec: node.spec.children[index]
                        Layout.fillWidth: childSpec.fillWidth === true
                        Layout.fillHeight: childSpec.fillHeight === true
                        Layout.preferredWidth: childSpec.preferredWidth !== undefined
                            ? childSpec.preferredWidth : implicitWidth
                        Layout.preferredHeight: childSpec.preferredHeight !== undefined
                            ? childSpec.preferredHeight : implicitHeight
                        source: Qt.resolvedUrl("Node.qml")
                        // mode and path MUST be bindings, not one-time assignments: setting
                        // `spec` below synchronously instantiates the whole child subtree, so
                        // nested leaves would otherwise capture node.mode/node.path before this
                        // parent's own mode/path have settled (default "run"/[]) and never update.
                        onLoaded: {
                            item.mode = Qt.binding(function () { return node.mode })
                            item.path = Qt.binding(function () { return node.path.concat([index]) })
                            item.selectedPath = Qt.binding(function () { return node.selectedPath })
                            item.selectRequested.connect(function (p) { node.selectRequested(p) })
                            item.spec = Qt.binding(function () { return node.spec.children[index] })
                        }
                    }
                }
            }

            // selection chrome (visual only — a Rectangle doesn't intercept input, so
            // it can stay on top without stealing clicks from the children)
            Rectangle {
                z: 2
                anchors.fill: parent
                color: "transparent"
                border.color: node.pathEq(node.path, node.selectedPath) ? "#2196f3" : "#e8e8e8"
                border.width: node.pathEq(node.path, node.selectedPath) ? 2 : 1
                visible: node.mode === "design"
            }
        }
    }

    // ---- leaf: the widget component named by spec.type ----------------------
    Component {
        id: leafComp
        Item {
            implicitWidth: w.item ? w.item.implicitWidth : 120
            implicitHeight: w.item ? w.item.implicitHeight : 60

            Loader {
                id: w
                anchors.fill: parent
                source: node.spec.type ? Qt.resolvedUrl(node.spec.type + ".qml") : ""
                onLoaded: {
                    item.spec = Qt.binding(function () { return node.spec })
                    item.value = Qt.binding(function () { return node.resolvedValue })
                    item.quality = Qt.binding(function () { return node.resolvedQuality })
                    if ("mode" in item) item.mode = Qt.binding(function () { return node.mode })
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                color: "transparent"
                border.color: "#2196f3"
                border.width: 2
                radius: 3
                visible: node.mode === "design" && node.pathEq(node.path, node.selectedPath)
            }
            MouseArea {
                anchors.fill: parent
                enabled: node.mode === "design"
                onClicked: node.selectRequested(node.path)
            }
        }
    }
}
