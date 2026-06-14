import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Editor for an opaque-object config field (e.g. a Modbus master's `device`). In
// the declarative model such a field is a reference: { ref = "<object name>" }.
// Pick an already-configured object of a compatible type, or build a new one via
// a dialog driven by that type's own schema. New objects are appended to the
// shared `objects` collection so the emitted config carries them too.
RowLayout {
    id: ref
    spacing: 6

    property string className          // e.g. "radapter::modbus::MasterDevice"
    property var values: ({})          // parent config object to write into
    property string valueKey           // field name in `values`
    property var schemas: ({})         // every worker schema (for the new-object form)
    property var objects: []           // shared configured-objects collection
    property int rev: 0                // bumped to refresh candidate list

    signal changed()                   // a reference was picked or a device created

    // which factory types satisfy a given opaque class
    readonly property var allowedMap: ({
        "radapter::modbus::MasterDevice": ["TcpModbusDevice", "RtuModbusDevice"],
        "radapter::modbus::SlaveDevice":  ["TcpModbusServer", "RtuModbusServer"]
    })
    function allowedTypes() { return allowedMap[className] || Object.keys(schemas) }
    function candidates() {
        var t = allowedTypes(), out = []
        for (var i = 0; i < objects.length; i++)
            if (t.indexOf(objects[i].type) >= 0) out.push(objects[i].name)
        return out
    }
    function modelList() { return (rev >= 0 ? candidates() : []).concat(["＋ New…"]) }

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

    Dialog {
        id: newDialog
        title: "New " + ref.className.split("::").pop()
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 440
        x: (ApplicationWindow.window ? ApplicationWindow.window.width - width : 0) / 2
        y: 40
        property var devValues: ({})

        onOpened: { devValues = {}; nameField.text = ""; typeBox.currentIndex = 0; subLoader.reload() }
        onAccepted: {
            var nm = nameField.text.trim()
            if (!nm.length) return
            ref.objects.push({ name: nm, type: typeBox.currentText, config: devValues })
            ref.rev += 1
            ref.values[ref.valueKey] = { ref: nm }
            combo.currentIndex = ref.candidates().indexOf(nm)
            ref.changed()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: 8
                Label { text: "Name" }
                TextField { id: nameField; Layout.fillWidth: true; placeholderText: "plc1" }
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
                        item.objects = ref.objects
                    }
                }
            }
        }
    }
}
