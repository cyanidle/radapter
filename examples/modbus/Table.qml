import QtQuick 2.7
import QtQuick.Controls 2.2

// One ModbusTable window. ModbusTable.qml is a sibling file, so it resolves by name
// without any module import. `win_title` is supplied as a context property from Lua
// (QML{ properties = { win_title = … } }).
ApplicationWindow {
    visible: true
    width: 560
    height: 300
    title: win_title

    ModbusTable {
        model: radapter.model.node("regs")   // the "regs" sub-tree
        anchors.fill: parent
        anchors.margins: 8
        Component.onCompleted: {
            addRow("speed",      0, "Holding", "Float32")
            addRow("setpoint",   2, "Holding")
            addRow("uptime",     0, "Input")
            addRow("enable",     0, "Coil")
            addRow("valve_open", 1, "Coil")
            addRow("running",    0, "Discrete")
        }
    }
}
