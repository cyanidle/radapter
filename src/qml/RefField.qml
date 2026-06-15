import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Editor for an opaque-object config field (e.g. a Modbus master's `device`). In
// the declarative model such a field is a reference: { ref = "<object name>" }.
// Pick an already-configured object of a compatible type, or build a new one via
// a dialog driven by that type's own schema. New objects are added to the shared
// ConfigContext so the emitted config carries them and their names stay unique.
RowLayout {
    id: ref
    spacing: 6

    property string className          // e.g. "radapter::modbus::MasterDevice"
    property var values: ({})          // parent config object to write into
    property string valueKey           // field name in `values`
    property var schemas: ({})         // every worker schema (for the new-object form)
    property var context: null         // shared ConfigContext (configured objects)

    signal changed()                   // a reference was picked or a device created

    // which factory types satisfy a given opaque class
    readonly property var allowedMap: ({
        "radapter::modbus::MasterDevice": ["TcpModbusDevice", "RtuModbusDevice"],
        "radapter::modbus::SlaveDevice":  ["TcpModbusServer", "RtuModbusServer"]
    })
    function allowedTypes() { return allowedMap[className] || Object.keys(schemas) }
    function candidates() { return context ? context.ofTypes(allowedTypes()) : [] }
    // read context.revision so the model refreshes when objects change
    function modelList() { return ((context ? context.revision : 0) >= 0 ? candidates() : []).concat(["＋ New…"]) }
    // index of the currently-referenced object in the candidate list (-1 if unset),
    // so the combo shows the saved ref when the form is rebuilt on reselect
    function currentRefIndex() {
        var cur = ref.values[ref.valueKey]
        if (cur === undefined || cur.ref === undefined) return -1
        return ref.candidates().indexOf(cur.ref)
    }

    ComboBox {
        id: combo
        Layout.fillWidth: true
        model: ref.modelList()
        onActivated: {
            var cands = ref.candidates()
            if (index < cands.length) { ref.values[ref.valueKey] = { ref: cands[index] }; ref.changed() }
            else newDialog.open()
        }
    }
    // restore/keep the display in sync with values[valueKey]: a user pick imperatively
    // sets currentIndex (breaking an inline binding), so a Binding element reasserts it,
    // and it re-applies after the form reloads on reselect (revision keeps it reactive)
    Binding {
        target: combo
        property: "currentIndex"
        value: (ref.context ? ref.context.revision : 0) >= 0 ? ref.currentRefIndex() : -1
    }

    Dialog {
        id: newDialog
        title: "New " + ref.className.split("::").pop()
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 440
        x: (ApplicationWindow.window ? ApplicationWindow.window.width - width : 0) / 2
        y: 40
        property var devValues: ({})
        readonly property string nameError: {
            var n = nameField.text.trim()
            var _rev = ref.context ? ref.context.revision : 0
            if (!n.length) return "Name is required"
            if (ref.context && ref.context.has(n)) return "Name already in use"
            return ""
        }

        // the standard button box may not exist yet when nameError first changes
        function syncOk() { var b = standardButton(Dialog.Ok); if (b) b.enabled = (nameError.length === 0) }

        onOpened: { devValues = {}; nameField.text = ""; typeBox.currentIndex = 0; subLoader.reload(); syncOk() }
        onAccepted: {
            var nm = nameField.text.trim()
            ref.context.put(nm, typeBox.currentText, devValues)
            ref.values[ref.valueKey] = { ref: nm }
            combo.currentIndex = ref.candidates().indexOf(nm)
            ref.changed()
        }
        Component.onCompleted: syncOk()
        onNameErrorChanged: syncOk()

        ColumnLayout {
            anchors.fill: parent
            spacing: 6
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: 8
                Label { text: "Name" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    TextField { id: nameField; Layout.fillWidth: true; placeholderText: "plc1" }
                    Label {
                        text: newDialog.nameError
                        visible: newDialog.nameError.length > 0
                        color: "#b00"
                        font.pixelSize: 11
                    }
                }
                Label { text: "Type" }
                ComboBox {
                    id: typeBox
                    Layout.fillWidth: true
                    model: ref.allowedTypes()
                    onActivated: subLoader.reload()
                }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                clip: true
                // loaded by URL so the QML compiler doesn't see the RefField<->SchemaForm cycle
                Loader {
                    id: subLoader
                    width: parent.width
                    function reload() {
                        newDialog.devValues = {}
                        source = ""
                        source = Qt.resolvedUrl("SchemaForm.qml")
                    }
                    onLoaded: {
                        item.schema = ref.schemas[typeBox.currentText] || ({})
                        item.values = newDialog.devValues
                        item.schemas = ref.schemas
                        item.context = ref.context
                    }
                }
            }
        }
    }
}
