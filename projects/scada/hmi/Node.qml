import QtQuick 2.7
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import "tree.js" as Tree

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

    // design mode: the in-container "＋" button offers these types to add as a child;
    // the request bubbles up to the editor (which mutates the tree at `path`).
    property var addTypes: []
    signal addRequested(var path, string type)

    // optional live overlay: a { tag: value } / { tag: quality } map. When set (the editor
    // canvas while a runner is streaming), a leaf shows the live value/quality for its tag
    // in place of the design placeholder.
    property var liveValues: undefined
    property var liveQuality: undefined

    // layout hints — applied to this Loader as seen by its parent layout
    Layout.fillWidth: spec.fillWidth === true
    Layout.fillHeight: spec.fillHeight === true
    Layout.preferredWidth: spec.preferredWidth !== undefined ? spec.preferredWidth : implicitWidth
    Layout.preferredHeight: spec.preferredHeight !== undefined ? spec.preferredHeight : implicitHeight
    Layout.minimumWidth: implicitWidth > 0 ? Math.min(implicitWidth, 40) : 0

    // resolved value/quality for leaf widgets. Only touch radapter.tags in run mode
    // (the editor process has no --tags, so radapter.tags is null there).
    readonly property var resolvedValue:
        spec.tag && liveValues && liveValues[spec.tag] !== undefined ? liveValues[spec.tag]
      : mode === "run" && spec.tag ? radapter.tags[spec.tag]
      : (mode === "design" ? designValue() : undefined)
    readonly property string resolvedQuality:
        spec.tag && liveQuality && liveQuality[spec.tag] !== undefined ? liveQuality[spec.tag]
      : mode === "run" && spec.tag ? (radapter.quality[spec.tag] || "comm_fail") : "good"

    // a representative placeholder so widgets look alive in the editor
    function designValue() {
        if (spec.min !== undefined && spec.max !== undefined)
            return (spec.min + spec.max) / 2
        return 42
    }

    Component.onCompleted: {
        if (mode === "run" && spec.tag && radapter.tags) radapter.tags.ensure(spec.tag)
    }

    sourceComponent: Tree.isContainer(spec.type) ? containerComp : leafComp

    // ---- container: a *Layout with a nested Node per child ------------------
    Component {
        id: containerComp
        Item {
            id: cont
            implicitWidth: content.item ? content.item.implicitWidth : 0
            implicitHeight: content.item ? content.item.implicitHeight : 0

            // Row wraps via a Flow; Column (one column) and Grid (N columns) use a
            // GridLayout, which already wraps and lets children fill the available width.
            readonly property bool useFlow: node.spec.type === "Row"

            // background click-catcher, BEHIND the children (z 0): clicking empty container
            // space selects this container; a click on a child widget is grabbed by the
            // child's own MouseArea, which sits above (z 1), so selection follows what you
            // clicked — and the deepest node under the cursor is the one that reports it.
            MouseArea {
                z: 0
                anchors.fill: parent
                enabled: node.mode === "design"
                onClicked: node.selectRequested(node.path)
            }

            Loader {
                id: content
                z: 1
                anchors.fill: parent
                sourceComponent: cont.useFlow ? flowComp : gridComp
            }

            // shared child renderer. In a Flow the child is sized by width/height; in the
            // GridLayout the Layout.* hints apply (and the layout overrides width/height).
            Component {
                id: childComp
                Loader {
                    property var childSpec: node.spec.children[index]
                    readonly property bool childIsContainer: Tree.isContainer(childSpec.type)
                    width: childSpec.preferredWidth !== undefined ? childSpec.preferredWidth : implicitWidth
                    height: childSpec.preferredHeight !== undefined ? childSpec.preferredHeight : implicitHeight
                    // a nested container fills the available width in a Column/Grid so its own
                    // Flow/Grid has a real width to wrap within (a Flow has no useful implicit
                    // width); leaf widgets keep their natural size unless they ask to fill.
                    Layout.fillWidth: childSpec.fillWidth === true || childIsContainer
                    Layout.fillHeight: childSpec.fillHeight === true
                    Layout.preferredWidth: childSpec.preferredWidth !== undefined ? childSpec.preferredWidth : implicitWidth
                    Layout.preferredHeight: childSpec.preferredHeight !== undefined ? childSpec.preferredHeight : implicitHeight
                    source: Qt.resolvedUrl("Node.qml")
                    // mode/path MUST be bindings: setting `spec` instantiates the child
                    // subtree synchronously, so it must not latch the defaults ("run"/[]).
                    onLoaded: {
                        item.mode = Qt.binding(function () { return node.mode })
                        item.path = Qt.binding(function () { return node.path.concat([index]) })
                        item.selectedPath = Qt.binding(function () { return node.selectedPath })
                        item.liveValues = Qt.binding(function () { return node.liveValues })
                        item.liveQuality = Qt.binding(function () { return node.liveQuality })
                        item.addTypes = Qt.binding(function () { return node.addTypes })
                        item.selectRequested.connect(function (p) { node.selectRequested(p) })
                        item.addRequested.connect(function (p, t) { node.addRequested(p, t) })
                        item.spec = Qt.binding(function () { return node.spec.children[index] })
                    }
                }
            }

            // design-mode "＋": append a child via a type-picker menu. Last in the layout,
            // so it sits to the right in a Row and at the bottom of a Column/Grid.
            Component {
                id: addComp
                Rectangle {
                    implicitWidth: 26; implicitHeight: 26
                    width: 26; height: 26
                    Layout.alignment: Qt.AlignCenter
                    radius: 4
                    color: addMouse.containsMouse ? "#e3f2fd" : "#f0f0f0"
                    border.color: "#bbb"; border.width: 1
                    Text { anchors.centerIn: parent; text: "＋"; color: "#1565c0"
                           font.pixelSize: 16; font.bold: true }
                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: addMenu.popup()
                    }
                    Menu {
                        id: addMenu

                        // ── Built-in widgets ───────────────────────────
                        MenuItem {
                            text: "Widgets"
                            enabled: false
                            font { bold: true; italic: true }
                        }
                        MenuItem {
                            text: "◎  Gauge"
                            onTriggered: node.addRequested(node.path, "Gauge")
                        }
                        MenuItem {
                            text: "ℹ  InfoDisplay"
                            onTriggered: node.addRequested(node.path, "InfoDisplay")
                        }
                        MenuItem {
                            text: "▤  Chart"
                            onTriggered: node.addRequested(node.path, "Chart")
                        }
                        MenuItem {
                            text: "◻  Spacer"
                            onTriggered: node.addRequested(node.path, "Spacer")
                        }

                        MenuSeparator {}

                        // ── Layout containers ──────────────────────────
                        MenuItem {
                            text: "Layout"
                            enabled: false
                            font { bold: true; italic: true }
                        }
                        MenuItem {
                            text: "↔  Row"
                            onTriggered: node.addRequested(node.path, "Row")
                        }
                        MenuItem {
                            text: "↕  Column"
                            onTriggered: node.addRequested(node.path, "Column")
                        }
                        MenuItem {
                            text: "⊞  Grid"
                            onTriggered: node.addRequested(node.path, "Grid")
                        }

                        MenuSeparator {}

                        // ── Custom component ───────────────────────────
                        MenuItem {
                            text: "⚙  Custom"
                            onTriggered: node.addRequested(node.path, "Custom")
                        }
                    }
                }
            }

            // Row: a Flow that wraps left-to-right when children exceed the width
            Component {
                id: flowComp
                Flow {
                    spacing: node.spec.spacing !== undefined ? node.spec.spacing : 6
                    flow: Flow.LeftToRight
                    Repeater {
                        model: node.spec.children ? node.spec.children.length : 0
                        delegate: childComp
                    }
                    Loader {                       // the "＋" button (design mode only)
                        active: node.mode === "design"
                        visible: node.mode === "design"
                        sourceComponent: addComp
                    }
                }
            }

            // Column (one column, stacks vertically) and Grid (N columns, wraps every N)
            Component {
                id: gridComp
                GridLayout {
                    rowSpacing: node.spec.spacing !== undefined ? node.spec.spacing : 6
                    columnSpacing: node.spec.spacing !== undefined ? node.spec.spacing : 6
                    flow: GridLayout.LeftToRight
                    columns: node.spec.type === "Grid"
                        ? (node.spec.columns !== undefined ? node.spec.columns : 2) : 1
                    Repeater {
                        model: node.spec.children ? node.spec.children.length : 0
                        delegate: childComp
                    }
                    Loader {
                        active: node.mode === "design"
                        visible: node.mode === "design"
                        sourceComponent: addComp
                    }
                }
            }

            // selection chrome (visual only — a Rectangle doesn't intercept input, so
            // it can stay on top without stealing clicks from the children)
            Rectangle {
                z: 2
                anchors.fill: parent
                color: "transparent"
                border.color: Tree.pathEq(node.path, node.selectedPath) ? "#2196f3" : "#e8e8e8"
                border.width: Tree.pathEq(node.path, node.selectedPath) ? 2 : 1
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
            // Explicit size ensures the leaf MouseArea always covers a real area,
            // even when the Loader chain's implicit-size propagation lags in Qt 5.
            width: implicitWidth
            height: implicitHeight

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
                visible: node.mode === "design" && Tree.pathEq(node.path, node.selectedPath)
            }
            MouseArea {
                z: 1   // above the widget Loader (z 0), so clicks hit selection first
                anchors.fill: parent
                enabled: node.mode === "design"
                onClicked: node.selectRequested(node.path)
            }
        }
    }
}
