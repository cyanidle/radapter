import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import radapter 1.0

// Schema-driven worker configurator window. The Lua side sends the worker schemas
// (filtered to the supported families) over the data channel; this window feeds
// them to a WorkerConfigurator and, when the user clicks "Build", emits the
// resulting declare-compatible config back to Lua via radapter.model.send.
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
    Component.onCompleted: radapter.model.received.connect(onMsg)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Label {
            text: "Configure a worker"
            font.bold: true
            font.pixelSize: 16
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
                types: root.pickable
                onEmitted: {
                    root.lastJson = JSON.stringify(fragment, null, 2)
                    radapter.model.send({ config: fragment })
                }
            }
        }

        Label { text: "Emitted config"; font.bold: true }
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
