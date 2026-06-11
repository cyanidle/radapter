import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Configurable table that visualizes Modbus register values.
//
// Add rows with "+", giving each a name, address and register type. Each row's
// value mirrors data delivered to this component under a field with the same
// name, e.g. piping `{ pump_speed = 1234 }` updates the row named "pump_speed"
// (a nested `{ pump_speed = { value = 1234, quality = .. } }` also works for
// extra fields). The "details" (three dots) popup configures endianness, data
// type (default: a 16-bit word) and hex display per row. Coil/Discrete rows show
// a checkbox (read-only for discrete inputs); numeric rows show editable text.
// Writable rows (Holding, Coil) can be edited — double-click text or toggle the
// checkbox — and the new value is sent out as `{ name = value }`.
Frame {
    id: root
    padding: 8
    implicitWidth: 520
    implicitHeight: 320

    readonly property var dataTypes: ["Word", "Int16", "UInt16", "Int32", "UInt32", "Float32", "Bool"]
    readonly property var endians: ["Big", "Little"]
    readonly property var regTypes: ["Holding", "Input", "Coil", "Discrete"]

    // value-holder objects keyed by register name. Each is a child QObject named
    // after the register; the GUI worker routes an incoming scalar `{ name = .. }`
    // into its declared (reactive) `value` property (or a nested map into its
    // fields), keeping list rows reactive without per-delegate routing.
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
            dataType: "Word",
            hex: false
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

    function formatValue(v, dataType, hex) {
        if (v === undefined || v === null) return "—"
        if (dataType === "Bool") return v ? "true" : "false"
        if (dataType === "Float32") return Number(v).toFixed(3)
        var n = Number(v)
        if (isNaN(n)) return String(v)
        if (hex) {
            var wide = (dataType === "Int32" || dataType === "UInt32")
            var u = wide ? (n >>> 0) : (n & 0xFFFF)
            var s = u.toString(16).toUpperCase()
            while (s.length < (wide ? 8 : 4)) s = "0" + s
            return "0x" + s
        }
        return String(n)
    }

    // parse user-typed text back into a value of the row's data type, or
    // undefined if it isn't valid (so an empty/garbage edit is ignored)
    function parseValue(text, dataType) {
        text = (text || "").trim()
        if (!text.length) return undefined
        if (dataType === "Bool") {
            var t = text.toLowerCase()
            return (t === "true" || t === "1" || t === "on")
        }
        var n = /^0x/i.test(text) ? parseInt(text, 16) : Number(text)
        if (isNaN(n)) return undefined
        return dataType === "Float32" ? n : Math.trunc(n)
    }

    function truthy(v) {
        return v === true || v === 1 || v === "true" || v === "1"
    }

    // write a value: update the holder locally (optimistic) and send it out the
    // worker's data channel as `{ name = value }`
    function sendWrite(name, v) {
        var holder = _holders[name]
        if (holder) holder.value = v
        if (typeof radapter !== "undefined" && radapter) {
            var msg = {}
            msg[name] = v
            radapter.sendMsg(msg)
        }
    }

    // commit an in-place text edit (parsed to the row's data type)
    function commitEdit(name, text, dataType) {
        var v = root.parseValue(text, dataType)
        if (v !== undefined) root.sendWrite(name, v)
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

                // value cell: a checkbox for bit registers (coils editable,
                // discrete inputs read-only), editable text otherwise (input
                // registers are read-only). Double-click text to edit it in
                // place (Enter/focus-out commits, Esc cancels).
                Item {
                    id: valueArea
                    Layout.fillWidth: true
                    implicitHeight: Math.max(editField.implicitHeight, bitCheck.implicitHeight)
                    property bool isBit: model.regType === "Coil" || model.regType === "Discrete"
                    property bool writable: model.regType === "Holding" || model.regType === "Coil"
                    property bool editing: false

                    CheckBox {
                        id: bitCheck
                        anchors.verticalCenter: parent.verticalCenter
                        visible: valueArea.isBit
                        enabled: valueArea.writable
                        // controlled checkbox: state is driven from the holder, not
                        // a binding (a user click would otherwise break it), so the
                        // box keeps tracking external updates after a local toggle
                        onToggled: root.sendWrite(model.name, checked)
                        Component.onCompleted: checked = rowItem.holder ? root.truthy(rowItem.holder.value) : false
                        Connections {
                            target: rowItem.holder
                            function onValueChanged() {
                                bitCheck.checked = root.truthy(rowItem.holder.value)
                            }
                        }
                    }

                    Label {
                        id: valLabel
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        visible: !valueArea.isBit && !valueArea.editing
                        elide: Text.ElideRight
                        text: rowItem.holder ? root.formatValue(rowItem.holder.value, model.dataType, model.hex) : "—"
                        MouseArea {
                            anchors.fill: parent
                            enabled: valueArea.writable
                            onDoubleClicked: {
                                editField.text = (rowItem.holder && rowItem.holder.value !== undefined)
                                                 ? String(rowItem.holder.value) : ""
                                valueArea.editing = true
                                editField.forceActiveFocus()
                                editField.selectAll()
                            }
                        }
                    }
                    TextField {
                        id: editField
                        anchors.fill: parent
                        visible: !valueArea.isBit && valueArea.editing
                        function commit() {
                            root.commitEdit(model.name, text, model.dataType)
                            valueArea.editing = false
                        }
                        onAccepted: commit()
                        onActiveFocusChanged: if (!activeFocus && valueArea.editing) commit()
                        Keys.onEscapePressed: valueArea.editing = false
                    }
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
            hexCheck.checked = r.hex === true
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

            CheckBox {
                id: hexCheck
                text: "Hex view"
                onToggled: if (detailsPopup.rowIndex >= 0)
                    regModel.setProperty(detailsPopup.rowIndex, "hex", checked)
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
