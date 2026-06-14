import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import radapter 1.0

// Schema-driven worker configurator window. The Lua side sends the worker schemas
// (filtered to the supported families); a drop-down picks which worker to create,
// the WorkerConfigurator panel configures that single worker, and a live preview
// shows just that worker's declarative config. "Build" sends the full (build-ready)
// fragment back to Lua. The per-worker panel is what a multi-object graph editor
// will later show when a node is selected.
ApplicationWindow {
    id: root
    visible: true
    width: 560
    height: 720
    title: "Radapter Configurator"

    property var schemas: ({})
    property var pickable: []
    property string lastJson: ""

    // schemas arrive as a plain { schemas: {...} } message; received() delivers the
    // raw tree as native JS (so Object.keys / arrays work, unlike the model tree)
    function onMsg(msg) {
        if (msg.schemas !== undefined) root.schemas = msg.schemas
        if (msg.pickable !== undefined) root.pickable = msg.pickable
    }
    function refreshPreview() {
        root.lastJson = JSON.stringify(configurator.currentWorker(), null, 2)
    }
    Component.onCompleted: radapter.model.received.connect(onMsg)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Label { text: "Create worker"; font.bold: true }
            ComboBox {
                id: typeBox
                Layout.fillWidth: true
                model: root.pickable
                onActivated: { configurator.type = currentText; root.refreshPreview() }
                Component.onCompleted: if (count > 0) configurator.type = currentText
            }
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
                type: typeBox.currentText
                onChanged: root.refreshPreview()
                onEmitted: radapter.model.send({ config: fragment })
            }
        }

        Label { text: "Preview (this worker)"; font.bold: true }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
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
