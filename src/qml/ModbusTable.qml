import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Configurable table that visualizes Modbus register values.
//
// Add rows with "+", giving each a name, address and register type. Each row's
// value mirrors data delivered to this component under a nested field with the
// same name, e.g. piping `{ pump_speed = { value = 1234 } }` updates the row
// named "pump_speed". The "details" (three dots) popup configures endianness
// and data type per row (default: a 16-bit word).
Frame {
    id: root
    padding: 8
    implicitWidth: 520
    implicitHeight: 320

    readonly property var dataTypes: ["Word", "Int16", "UInt16", "Int32", "UInt32", "Float32", "Bool"]
    readonly property var endians: ["Big", "Little"]
    readonly property var regTypes: ["Holding", "Input", "Coil", "Discrete"]

    // value-holder objects keyed by register name. Each is a child QObject named
    // after the register so that an incoming `{ name = { value = .. } }` message
    // is routed into its declared (reactive) `value` property by the GUI worker.
    property var _holders: ({})

    Component {
        id: holderComp
        QtObject {
            property var value
            property string quality: ""
        }
    }

    function addRow(name, address, regType) {
        name = (name || "").trim()
        if (!name.length || _holders[name])
            return
        var holder = holderComp.createObject(root, { objectName: name })
        var h = {}
        for (var k in _holders) h[k] = _holders[k]
        h[name] = holder
        _holders = h
        regModel.append({
            name: name,
            address: String(address || ""),
            regType: regType || "Holding",
            endian: "Big",
            dataType: "Word"
        })
    }

    function removeRow(index) {
        if (index < 0 || index >= regModel.count) return
        var name = regModel.get(index).name
        var holder = _holders[name]
        var h = {}
        for (var k in _holders) if (k !== name) h[k] = _holders[k]
        _holders = h
        regModel.remove(index)
        if (holder) holder.destroy()
    }

    function formatValue(v, dataType) {
        if (v === undefined || v === null) return "—"
        if (dataType === "Float32") return Number(v).toFixed(3)
        if (dataType === "Bool") return v ? "true" : "false"
        if (dataType === "Word") {
            var n = Number(v)
            if (!isNaN(n)) return n + "  (0x" + (n & 0xFFFF).toString(16).toUpperCase() + ")"
        }
        return String(v)
    }

    ListModel { id: regModel }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Modbus Registers"
                font.bold: true
                font.pixelSize: 15
                Layout.fillWidth: true
            }
            Button {
                text: "+"
                implicitWidth: 36
                onClicked: addDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "Name";    font.bold: true; Layout.preferredWidth: 150; elide: Text.ElideRight }
            Label { text: "Address"; font.bold: true; Layout.preferredWidth: 80 }
            Label { text: "Type";    font.bold: true; Layout.preferredWidth: 80 }
            Label { text: "Value";   font.bold: true; Layout.fillWidth: true }
            Item  { Layout.preferredWidth: 36 }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#888" }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: regModel
            spacing: 2

            delegate: RowLayout {
                id: rowItem
                width: ListView.view ? ListView.view.width : implicitWidth
                spacing: 8
                property QtObject holder: root._holders[model.name] || null

                Label { text: model.name;    Layout.preferredWidth: 150; elide: Text.ElideRight }
                Label { text: model.address; Layout.preferredWidth: 80 }
                Label { text: model.regType; Layout.preferredWidth: 80 }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: rowItem.holder ? root.formatValue(rowItem.holder.value, model.dataType) : "—"
                }
                Button {
                    text: "⋮"
                    implicitWidth: 36
                    onClicked: {
                        detailsPopup.rowIndex = index
                        var p = mapToItem(root, 0, height)
                        detailsPopup.x = Math.min(p.x, root.availableWidth - detailsPopup.implicitWidth)
                        detailsPopup.y = p.y
                        detailsPopup.open()
                    }
                }
            }
        }
    }

    // Add-row dialog: name, address, register type.
    Dialog {
        id: addDialog
        title: "Add register"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            root.addRow(nameField.text, addrField.text, regTypeBox.currentText)
            nameField.text = ""
            addrField.text = ""
            regTypeBox.currentIndex = 0
        }

        GridLayout {
            columns: 2
            columnSpacing: 8
            rowSpacing: 8
            Label { text: "Name" }
            TextField { id: nameField; Layout.fillWidth: true; placeholderText: "pump_speed" }
            Label { text: "Address" }
            TextField {
                id: addrField
                Layout.fillWidth: true
                placeholderText: "0"
                inputMethodHints: Qt.ImhDigitsOnly
            }
            Label { text: "Register" }
            ComboBox { id: regTypeBox; Layout.fillWidth: true; model: root.regTypes }
        }
    }

    // Per-row details: endianness, data type, remove.
    Popup {
        id: detailsPopup
        property int rowIndex: -1
        modal: true
        focus: true
        padding: 12

        onOpened: {
            if (rowIndex < 0) return
            var r = regModel.get(rowIndex)
            endianBox.currentIndex = Math.max(0, root.endians.indexOf(r.endian))
            typeBox.currentIndex = Math.max(0, root.dataTypes.indexOf(r.dataType))
        }

        ColumnLayout {
            spacing: 8
            Label { text: "Register details"; font.bold: true }

            GridLayout {
                columns: 2
                columnSpacing: 8
                rowSpacing: 6
                Label { text: "Endianness" }
                ComboBox {
                    id: endianBox
                    Layout.preferredWidth: 150
                    model: root.endians
                    onActivated: if (detailsPopup.rowIndex >= 0)
                        regModel.setProperty(detailsPopup.rowIndex, "endian", currentText)
                }
                Label { text: "Data type" }
                ComboBox {
                    id: typeBox
                    Layout.preferredWidth: 150
                    model: root.dataTypes
                    onActivated: if (detailsPopup.rowIndex >= 0)
                        regModel.setProperty(detailsPopup.rowIndex, "dataType", currentText)
                }
            }

            Button {
                text: "Remove row"
                Layout.fillWidth: true
                onClicked: {
                    root.removeRow(detailsPopup.rowIndex)
                    detailsPopup.close()
                }
            }
        }
    }
}
