-- Visualize live register values in a configurable table (radapter.ModbusTable).
--
-- Run with:
--   build/bin/radapter --gui examples/modbus_table.lua
--
-- Use the "+" button to add rows (name, address, register type); the three-dots
-- popup on each row configures endianness, data type (default: 16-bit word) and
-- hex display. A row shows whatever value is delivered to the table under a field
-- with the same name (e.g. `{ pump_speed = 1500 }`). A nested
-- `{ pump_speed = { value = .., quality = .. } }` works too when you need extra
-- fields. Double-click a value to edit it in place; the edit is sent out as
-- `{ name = value }` (logged below). Here a timer feeds the live rows, while
-- "setpoint" is left for you to edit.

local view = QML[[
import QtQuick 2.7
import QtQuick.Controls 2.2
import radapter 1.0

ApplicationWindow {
    visible: true
    width: 560; height: 360
    title: "Modbus register table"

    ModbusTable {
        objectName: "regs"
        anchors.fill: parent
        anchors.margins: 8
        Component.onCompleted: {
            addRow("pump_speed",  3, "Holding")
            addRow("pump_status", 1, "Holding")
            addRow("temperature", 7, "Input")
            addRow("setpoint",   10, "Holding")
        }
    }
}
]]

-- log values written from the table (in-place edits)
pipe(view, function(msg)
    for k, v in pairs(msg) do
        log.info("write {} = {}", k, v)
    end
end)

local t = 0
each(500, function()
    t = t + 1
    -- nested under the table's objectName, then a flat value per register
    view{ regs = {
        pump_speed  = 1500 + (t % 10) * 25,
        pump_status = t % 2,
        temperature = 20 + (t % 5),
    }}
end)
