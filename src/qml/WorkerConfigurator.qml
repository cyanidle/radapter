import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Properties panel for a *single* worker of a given `type`: a name field plus a
// schema-driven form. The type is set externally (so a host can pick which worker
// to configure, and later reuse this as the per-node panel of a graph editor).
//
// All editors sharing one `context` (a ConfigContext) see a single set of authored
// objects: names are validated unique across them, and devices created via a RefField
// land in the same store. The worker is registered into `context` live as it is edited.
//
// - currentWorker() -> { <name> = { type, config } } (just this worker; for preview)
// - currentFragment() -> declare-compatible { objects = { ... } } from the whole context
ColumnLayout {
    id: cfg
    spacing: 8

    property var schemas: ({})     // every worker schema, keyed by type name
    property string type: ""       // the worker type to configure (set externally)
    property var context: null     // shared ConfigContext (objects + name uniqueness)
    property var customForms: ({}) // per-field editor overrides (see SchemaForm)

    property var values: ({})      // this worker's config, filled by the form
    property string registeredName: ""  // the name this editor currently occupies

    signal changed()               // fires on any edit (name or a form field)

    function name() { return nameField.text.trim() }
    function setName(name) { nameField.text = name }

    // "" when the name is valid; otherwise the reason. Reactive via nameField.text,
    // context.revision and registeredName.
    readonly property string nameError: {
        var n = nameField.text.trim()
        var _rev = context ? context.revision : 0
        if (!n.length) return "Name is required"
        if (context && context.has(n) && n !== registeredName) return "Name already in use"
        return ""
    }
    readonly property bool nameOk: nameError.length === 0

    // claim our name in the shared context (or release it if the name is invalid).
    // Validity is recomputed here from the current name rather than read off the nameOk
    // binding, which may still be stale when called synchronously from onTextChanged.
    function register() {
        if (!context) return
        var n = name()
        var ok = n.length > 0 && (!context.has(n) || n === registeredName)
        if (!ok) {
            if (registeredName.length) { context.remove(registeredName); registeredName = "" }
            return
        }
        if (registeredName.length && registeredName !== n) context.remove(registeredName)
        context.put(n, cfg.type, values)
        registeredName = n
    }

    function currentWorker() {
        var e = {}
        e[name() || "(unnamed)"] = { type: cfg.type, config: values }
        return e
    }
    // the whole authored set (this worker + any device objects), ready for declare.build
    function currentFragment() {
        register()
        return { objects: context ? context.objects : currentWorker() }
    }

    // reset the form when the configured type changes
    onTypeChanged: { values = {}; register(); formLoader.reload(); changed() }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Label { text: "Name"; font.bold: true; Layout.preferredWidth: 150 }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "my_worker"
                onTextChanged: { cfg.register(); cfg.changed() }
            }
            Label {
                text: cfg.nameError
                visible: cfg.nameError.length > 0
                color: "#b00"
                font.pixelSize: 11
            }
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
            context: cfg.context
            customForms: cfg.customForms
            path: cfg.type + "."
            onChanged: cfg.changed()
        }
    }

    Component.onCompleted: formLoader.reload()
}
