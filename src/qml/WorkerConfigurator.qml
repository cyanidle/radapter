import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Single-worker configurator: a worker-type picker + name + a schema-driven form.
// Drives `objects` (the shared configured-objects collection, also used by device
// RefFields) and, on "Build", emits a declare-compatible fragment
//   { objects = { <name> = { type, config } [, <device> = {...}] } }
// via the `emitted` signal (the example wires it to radapter.model.send).
ColumnLayout {
    id: cfg
    spacing: 8

    property var schemas: ({})     // every worker schema, keyed by type name
    property var types: []         // restrict the type picker (empty = all schemas)
    property var objects: []       // configured objects, accumulated across builds

    signal emitted(var fragment)

    property var values: ({})      // the current worker's config, filled by the form

    function typeNames() {
        var ks = (types && types.length) ? types.slice() : Object.keys(schemas)
        ks.sort()
        return ks
    }
    function currentType() { return typeBox.currentText }

    // imperative API (also handy for tests/automation)
    function selectType(name) {
        var i = typeNames().indexOf(name)
        if (i < 0) return
        typeBox.currentIndex = i
        values = {}
        formLoader.reload()
    }
    function setName(name) { nameField.text = name }
    function buildNow() {
        var nm = nameField.text.trim()
        if (!nm.length || !currentType().length) return
        var objs = {}
        for (var i = 0; i < objects.length; i++)
            objs[objects[i].name] = { type: objects[i].type, config: objects[i].config }
        objs[nm] = { type: currentType(), config: values }
        emitted({ objects: objs })
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Label { text: "Worker"; font.bold: true }
        ComboBox {
            id: typeBox
            Layout.fillWidth: true
            model: cfg.typeNames()
            onActivated: { cfg.values = {}; formLoader.reload() }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Label { text: "Name"; Layout.preferredWidth: 150 }
        TextField { id: nameField; Layout.fillWidth: true; placeholderText: "my_worker" }
    }

    Rectangle { Layout.fillWidth: true; height: 1; color: "#bbb" }

    Loader {
        id: formLoader
        Layout.fillWidth: true
        Layout.fillHeight: true
        function reload() { sourceComponent = null; sourceComponent = formComp }
    }

    Component {
        id: formComp
        SchemaForm {
            schema: cfg.schemas[cfg.currentType()] || ({})
            values: cfg.values
            schemas: cfg.schemas
            objects: cfg.objects
        }
    }

    Button {
        text: "Build"
        Layout.fillWidth: true
        enabled: nameField.text.trim().length > 0 && cfg.currentType().length > 0
        onClicked: cfg.buildNow()
    }

    Component.onCompleted: formLoader.reload()
}
