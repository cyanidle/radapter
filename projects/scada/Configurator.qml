import QtQuick 2.7
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
// configurator components (WorkerGraph, WorkerConfigurator, ConfigContext, …) are
// sibling .qml files in this directory and resolve by name without an import.

// Multi-object graph configurator. The Lua side sends the worker schemas (filtered to
// the supported families). The toolbar adds workers; the WorkerGraph canvas shows every
// configured object grouped by type, each with a badge of how many pipes touch it.
// Clicking a node opens it in the WorkerConfigurator panel below; dragging a node's nub
// onto another worker links them into a pipe. A live preview shows the authored
// declarative config (objects + pipes), ready for declare.build / declare.save_to.
// The canvas, editor and preview are arranged in SplitViews so the panes are resizable.
ApplicationWindow {
    id: root
    visible: true
    width: 720
    height: 860
    title: "Radapter Configurator"

    property var schemas: ({})
    property var pickable: []
    property var formOverrides: ({})   // per-field custom editors, from Lua
    property string lastJson: ""
    property string runState: "—"      // runner connection state, shown in the log window

    // schemas arrive as a plain { schemas: {...} } message; received() delivers the
    // raw tree as native JS (so Object.keys / arrays work, unlike the model tree)
    function onMsg(msg) {
        if (msg.schemas !== undefined) root.schemas = msg.schemas
        if (msg.pickable !== undefined) root.pickable = msg.pickable
        // a declare-style { objects, pipes } config to seed/replace the editor's set
        if (msg.config !== undefined) sharedContext.load(msg.config)
        // streamed back from the headless runner launched by "Run"
        if (msg.run_state !== undefined) root.runState = String(msg.run_state)
        if (msg.log !== undefined) logWindow.append(msg.log)
    }

    // shared authoring state: the WorkerConfigurator and the WorkerGraph both read/write
    // this single set of objects (+ pipes), keeping names unique and edges consistent.
    ConfigContext { id: sharedContext }

    function refreshPreview() {
        var _rev = sharedContext.revision   // re-run when the shared set changes
        root.lastJson = JSON.stringify({ objects: sharedContext.objects,
                                         pipes: sharedContext.pipes }, null, 2)
    }

    function uniqueName(type) {
        var base = type.charAt(0).toLowerCase() + type.slice(1)
        var i = 1, n
        do { n = base + i; i++ } while (sharedContext.has(n))
        return n
    }
    function addWorker(type) {
        if (!type.length) return
        var n = uniqueName(type)
        sharedContext.put(n, type, {})
        configurator.select(n)
        graph.selected = n
    }

    Component.onCompleted: {
        radapter.model.received.connect(onMsg)
        sharedContext.changed.connect(refreshPreview)
        // custom_forms is a global context property set from Lua (QML properties=...)
        if (typeof custom_forms !== "undefined") root.formOverrides = custom_forms
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Label { text: "Add worker"; font.bold: true }
            ComboBox {
                id: typeBox
                Layout.preferredWidth: 200
                model: root.pickable
            }
            Button {
                text: "＋ Add"
                enabled: typeBox.currentText.length > 0
                onClicked: root.addWorker(typeBox.currentText)
            }
            ToolSeparator {}
            Button {
                text: "▶ Run"
                // depend on revision so it re-evaluates as objects are added/removed
                enabled: sharedContext.revision >= 0
                         && Object.keys(sharedContext.objects).length > 0
                onClicked: {
                    radapter.model.send({ run: { objects: sharedContext.objects,
                                                 pipes: sharedContext.pipes } })
                    logWindow.show()
                    logWindow.raise()
                }
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "Drag the ● on a worker onto another to connect"
                color: "#888"
                font.pixelSize: 11
            }
        }

        // graph canvas on the left; the three editor panes docked on the right
        SplitView {
            orientation: Qt.Horizontal
            Layout.fillWidth: true
            Layout.fillHeight: true

            WorkerGraph {
                id: graph
                SplitView.fillWidth: true
                SplitView.minimumWidth: 240
                context: sharedContext
                connectableTypes: root.pickable   // workers connect; devices (refs) don't
                onNodeClicked: configurator.select(name)
                onNodeRemoved: if (configurator.registeredName === name) configurator.clear()
            }

            // worker config / pipe list / preview, stacked and snapped to the right
            SplitView {
                orientation: Qt.Vertical
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 220

                ColumnLayout {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 120
                    spacing: 6

                    Label {
                        text: configurator.registeredName.length
                              ? ("Editing: " + configurator.registeredName)
                              : "Select or add a worker to edit"
                        font.bold: true
                    }

                    ScrollView {
                        id: scroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        WorkerConfigurator {
                            id: configurator
                            width: scroll.availableWidth
                            schemas: root.schemas
                            context: sharedContext
                            customForms: root.formOverrides
                            onChanged: root.refreshPreview()
                        }
                    }
                }

                // connections (pipes) of the selected worker — devices (anything not in
                // the pickable/connectable set) have no pipes, so don't show the panel
                ColumnLayout {
                    SplitView.preferredHeight: 200
                    SplitView.minimumHeight: 80
                    visible: configurator.registeredName.length > 0
                             && root.pickable.indexOf(configurator.type) !== -1
                    spacing: 4

                    Label { text: "Connections"; font.bold: true }
                    Frame {
                        id: connFrame
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ConnectionsList {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            context: sharedContext
                            worker: configurator.registeredName
                        }
                    }
                }

                ColumnLayout {
                    SplitView.preferredHeight: 200
                    SplitView.minimumHeight: 60
                    spacing: 4

                    Label { text: "Preview (objects + pipes)"; font.bold: true }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        TextArea {
                            readOnly: true
                            wrapMode: TextEdit.NoWrap
                            text: root.lastJson
                            font.family: "monospace"
                        }
                    }
                }
            }
        }
    }

    // Separate window showing the logs streamed back from the headless runner
    // launched by "Run". Closing it (or pressing Stop) shuts the runner down.
    ApplicationWindow {
        id: logWindow
        visible: false
        width: 660
        height: 440
        title: "Runner — " + root.runState

        function append(entry) {
            logModel.append({ lvl: String(entry.level),
                              body: String(entry.msg),
                              cat: String(entry.category) })
            logList.positionViewAtEnd()
        }

        onClosing: radapter.model.send({ stop: true })

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label { text: "State: " + root.runState; font.bold: true }
                Item { Layout.fillWidth: true }
                Button { text: "Clear"; onClicked: logModel.clear() }
                Button {
                    text: "■ Stop"
                    onClicked: { radapter.model.send({ stop: true }); root.runState = "stopped" }
                }
            }

            ListView {
                id: logList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ListModel { id: logModel }
                onCountChanged: positionViewAtEnd()

                delegate: Row {
                    width: logList.width
                    spacing: 8
                    Text {
                        text: model.cat
                        color: "#999"
                        font.family: "monospace"
                        font.pixelSize: 11
                        width: 160
                        elide: Text.ElideLeft
                    }
                    Text {
                        text: model.body
                        width: logList.width - 168
                        wrapMode: Text.Wrap
                        font.family: "monospace"
                        font.pixelSize: 11
                        color: model.lvl === "error" ? "#c62828"
                             : model.lvl === "warn"  ? "#ef6c00"
                             : model.lvl === "debug" ? "#9e9e9e" : "#222"
                    }
                }
            }
        }
    }
}
