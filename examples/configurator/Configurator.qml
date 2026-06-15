import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import radapter 1.0

// Multi-object graph configurator. The Lua side sends the worker schemas (filtered to
// the supported families). The toolbar adds workers; the WorkerGraph canvas shows every
// configured object grouped by type, each with a badge of how many pipes touch it.
// Clicking a node opens it in the WorkerConfigurator panel below; "Connect" mode links
// two nodes into a pipe. A live preview shows the authored declarative config (objects +
// pipes), ready for declare.build / declare.save_to on the Lua side.
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

    // schemas arrive as a plain { schemas: {...} } message; received() delivers the
    // raw tree as native JS (so Object.keys / arrays work, unlike the model tree)
    function onMsg(msg) {
        if (msg.schemas !== undefined) root.schemas = msg.schemas
        if (msg.pickable !== undefined) root.pickable = msg.pickable
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
            Item { Layout.fillWidth: true }
            // custom contentItem/background so the label stays legible in the active
            // (checked) state regardless of the Qt Quick Controls style in use
            Button {
                id: connectBtn
                checkable: true
                checked: graph.connectMode
                onToggled: graph.connectMode = checked
                implicitWidth: Math.max(110, contentLabel.implicitWidth + 24)
                text: graph.connectMode
                      ? (graph.connectFrom.length ? "Pick target…" : "Pick source…")
                      : "Connect"
                contentItem: Label {
                    id: contentLabel
                    text: connectBtn.text
                    color: connectBtn.checked ? "white" : "#222"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 4
                    color: connectBtn.checked ? "#fb8c00"
                           : (connectBtn.down ? "#d0d0d0" : "#e4e4e4")
                    border.color: "#bbb"
                }
            }
        }

        WorkerGraph {
            id: graph
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            context: sharedContext
            onNodeClicked: configurator.select(name)
            onNodeRemoved: if (configurator.registeredName === name) configurator.clear()
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#bbb" }

        Label {
            text: configurator.registeredName.length
                  ? ("Editing: " + configurator.registeredName)
                  : "Select or add a worker to edit"
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

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

            // connections of the selected worker, alongside its config form
            Frame {
                Layout.preferredWidth: 230
                Layout.fillHeight: true
                visible: configurator.registeredName.length > 0
                ConnectionsList {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    context: sharedContext
                    worker: configurator.registeredName
                }
            }
        }

        Label { text: "Preview (objects + pipes)"; font.bold: true }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
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
