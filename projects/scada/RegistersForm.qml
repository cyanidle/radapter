import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Built-in custom editor for a Modbus registers map (ModbusMaster/ModbusSlave
// `.registers`). The auto-generated nested map-of-maps form is awkward to fill, so this
// presents a flat table of rows that compiles into the nested declarative shape:
//   { holding = { <name> = { index, type } }, coils = {...}, di = {...}, input = {...} }
// SchemaForm applies it automatically for the registers field (see its builtinForms).
// Contract for a custom field editor:
//   properties: fkey, fschema, values, schemas, context ; signal changed()
//   it edits values[fkey] and emits changed() on every edit.
ColumnLayout {
    id: regForm
    spacing: 4

    property string fkey
    property var fschema
    property var values: ({})
    property var schemas: ({})
    property var context: null
    signal changed()

    readonly property var regTypes: ["holding", "coils", "di", "input"]
    readonly property var dataTypes: ["uint16", "uint32", "float32"]

    ListModel { id: rows }

    // compile the table into values[fkey] and notify
    function rebuild() {
        var out = {}
        for (var i = 0; i < rows.count; i++) {
            var r = rows.get(i)
            var nm = (r.rname || "").trim()
            if (!nm.length) continue
            if (!out[r.regType]) out[r.regType] = {}
            out[r.regType][nm] = { index: parseInt(r.addr, 10), type: r.dataType }
        }
        regForm.values[regForm.fkey] = out
        regForm.changed()
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: "Name"; font.bold: true; Layout.preferredWidth: 130 }
        Label { text: "Type"; font.bold: true; Layout.preferredWidth: 90 }
        Label { text: "Index"; font.bold: true; Layout.preferredWidth: 70 }
        Label { text: "Data"; font.bold: true; Layout.fillWidth: true }
        Item { Layout.preferredWidth: 32 }
    }

    Repeater {
        model: rows
        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 4
            // capture index + the combo roles here, where the delegate's `model` context
            // isn't shadowed: inside a ComboBox `model` is its own item model, and
            // `onActivated(index)` shadows the context index with the activated item's
            property int row: index
            property string regTypeRole: model.regType
            property string dataTypeRole: model.dataType
            TextField {
                Layout.preferredWidth: 130
                placeholderText: "pump_speed"
                text: model.rname
                onEditingFinished: { rows.setProperty(row, "rname", text); regForm.rebuild() }
            }
            ComboBox {
                Layout.preferredWidth: 90
                model: regForm.regTypes
                currentIndex: regForm.regTypes.indexOf(regTypeRole)
                onActivated: { rows.setProperty(row, "regType", currentText); regForm.rebuild() }
            }
            TextField {
                Layout.preferredWidth: 70
                placeholderText: "0"
                inputMethodHints: Qt.ImhDigitsOnly
                text: model.addr
                onEditingFinished: { rows.setProperty(row, "addr", text); regForm.rebuild() }
            }
            ComboBox {
                Layout.fillWidth: true
                model: regForm.dataTypes
                currentIndex: regForm.dataTypes.indexOf(dataTypeRole)
                onActivated: { rows.setProperty(row, "dataType", currentText); regForm.rebuild() }
            }
            // capture the root id before remove(): removing destroys this delegate, after
            // which `regForm` no longer resolves through its (torn-down) context
            Button { text: "✕"; implicitWidth: 32; onClicked: { var f = regForm; rows.remove(row); f.rebuild() } }
        }
    }

    Button {
        text: "＋ Add register"
        onClicked: rows.append({ rname: "", regType: "holding", addr: "0", dataType: "uint16" })
    }
}
