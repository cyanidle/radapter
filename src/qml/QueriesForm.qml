import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Built-in custom editor for a ModbusMaster `queries` vector. The auto-generated
// list-of-forms is clumsy for a handful of identical rows, so this presents a flat table
// that compiles into the declarative shape:
//   [ { type, index, count }, ... ]
// SchemaForm applies it automatically for the queries field (see its builtinForms).
// Contract for a custom field editor:
//   properties: fkey, fschema, values, schemas, context ; signal changed()
//   it edits values[fkey] and emits changed() on every edit.
ColumnLayout {
    id: qForm
    spacing: 4

    property string fkey
    property var fschema
    property var values: ({})
    property var schemas: ({})
    property var context: null
    signal changed()

    readonly property var queryTypes: ["holding", "coil", "di", "discrete_input", "input"]

    ListModel { id: rows }

    // compile the table into values[fkey] (an array) and notify
    function rebuild() {
        var out = []
        for (var i = 0; i < rows.count; i++) {
            var r = rows.get(i)
            out.push({
                type: r.qtype,
                index: parseInt(r.addr, 10) || 0,
                count: parseInt(r.cnt, 10) || 1
            })
        }
        qForm.values[qForm.fkey] = out
        qForm.changed()
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: "Type"; font.bold: true; Layout.preferredWidth: 130 }
        Label { text: "Index"; font.bold: true; Layout.preferredWidth: 90 }
        Label { text: "Count"; font.bold: true; Layout.fillWidth: true }
        Item { Layout.preferredWidth: 32 }
    }

    Repeater {
        model: rows
        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 4
            // capture index + the combo role here, where the delegate's `model` context
            // isn't shadowed: inside a ComboBox `model` is its own item model, and
            // `onActivated(index)` shadows the context index with the activated item's
            property int row: index
            property string qtypeRole: model.qtype
            ComboBox {
                Layout.preferredWidth: 130
                model: qForm.queryTypes
                currentIndex: Math.max(0, qForm.queryTypes.indexOf(qtypeRole))
                onActivated: { rows.setProperty(row, "qtype", currentText); qForm.rebuild() }
            }
            TextField {
                Layout.preferredWidth: 90
                placeholderText: "0"
                inputMethodHints: Qt.ImhDigitsOnly
                text: model.addr
                onEditingFinished: { rows.setProperty(row, "addr", text); qForm.rebuild() }
            }
            TextField {
                Layout.fillWidth: true
                placeholderText: "1"
                inputMethodHints: Qt.ImhDigitsOnly
                text: model.cnt
                onEditingFinished: { rows.setProperty(row, "cnt", text); qForm.rebuild() }
            }
            // capture the root id before remove(): removing destroys this delegate, after
            // which `qForm` no longer resolves through its (torn-down) context
            Button { text: "✕"; implicitWidth: 32; onClicked: { var f = qForm; rows.remove(row); f.rebuild() } }
        }
    }

    Button {
        text: "＋ Add query"
        onClicked: rows.append({ qtype: "holding", addr: "0", cnt: "1" })
    }
}
