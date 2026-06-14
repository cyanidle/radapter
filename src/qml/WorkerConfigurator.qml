import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Properties panel for a *single* worker of a given `type`: a name field plus a
// schema-driven form. The type is set externally (so a host can pick which worker
// to configure, and later reuse this as the per-node panel of a graph editor).
//
// - currentWorker() -> { <name> = { type, config } } (just this worker; for preview)
// - currentFragment() -> declare-compatible { objects = { <name>, <devices…> } }
//   including any device objects created via a RefField; emitted on Build.
ColumnLayout {
    id: cfg
    spacing: 8

    property var schemas: ({})     // every worker schema, keyed by type name
    property string type: ""       // the worker type to configure (set externally)
    property var objects: []       // shared configured objects (for device refs)
    property var customForms: ({}) // per-field editor overrides (see SchemaForm)

    property var values: ({})      // this worker's config, filled by the form

    signal emitted(var fragment)
    signal changed()               // fires on any edit (name or a form field)

    function setName(name) { nameField.text = name }
    function name() { return nameField.text.trim() }

    function currentWorker() {
        var e = {}
        e[name() || "(unnamed)"] = { type: cfg.type, config: values }
        return e
    }
    // worker + any device objects referenced by it, ready for declare.build
    function currentFragment() {
        var objs = {}
        for (var i = 0; i < objects.length; i++)
            objs[objects[i].name] = { type: objects[i].type, config: objects[i].config }
        objs[name() || "(unnamed)"] = { type: cfg.type, config: values }
        return { objects: objs }
    }
    function buildNow() {
        if (name().length && cfg.type.length) emitted(currentFragment())
    }

    // reset the form when the configured type changes
    onTypeChanged: { values = {}; formLoader.reload(); changed() }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Label { text: "Name"; font.bold: true; Layout.preferredWidth: 150 }
        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "my_worker"
            onTextChanged: cfg.changed()
        }
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
            schema: cfg.schemas[cfg.type] || ({})
            values: cfg.values
            schemas: cfg.schemas
            objects: cfg.objects
            customForms: cfg.customForms
            path: cfg.type + "."
            onChanged: cfg.changed()
        }
    }

    Button {
        text: "Build"
        Layout.fillWidth: true
        enabled: cfg.name().length > 0 && cfg.type.length > 0
        onClicked: cfg.buildNow()
    }

    Component.onCompleted: formLoader.reload()
}
