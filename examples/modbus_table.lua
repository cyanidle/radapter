-- Two ModbusMasters talk to one local Modbus TCP slave (server), each driving
-- its own radapter.ModbusTable window. A write in one window travels master ->
-- slave -> the other master, so both windows stay in sync after a poll cycle.
--
-- Covers all four register types:
--   holding (read/write words & floats), input (read-only words),
--   coils (read/write bits, shown as an editable checkbox),
--   di / discrete inputs (read-only bits, shown as a read-only checkbox).
--
-- Run with:
--   build/bin/radapter --gui examples/modbus_table.lua

local PORT = 15020

local registers = {
    holding = {
        speed    = { index = 0, type = "float32" },
        setpoint = { index = 2 },
    },
    input = {
        uptime = { index = 0 },
    },
    coils = {
        enable = { index = 0 },
        valve_open = { index = 1 },
    },
    di = {
        running = { index = 0 },
    },
}

-- Local slave (server). Seed initial values, then drive the read-only registers
-- (an input register and a discrete input) so both windows show them changing.
local slave = ModbusSlave {
    device = TcpModbusServer { host = "0.0.0.0", port = PORT },
    slave_id = 1,
    registers = registers,
}

slave { speed = 12.5, setpoint = 1000, enable = false, valve_open = false, uptime = 0, running = false }

local up = 0
each(1000, function()
    up = up + 1
    slave { uptime = up, running = (up % 2 == 0) }
end)

-- Build a window with a ModbusTable pre-populated with one row per register.
local function make_window(title)
    return QML([[
import QtQuick 2.7
import QtQuick.Controls 2.2
import radapter 1.0

ApplicationWindow {
    visible: true
    width: 560; height: 300
    title: "]] .. title .. [["

    ModbusTable {
        objectName: "regs"
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
]])
end

-- Connect a fresh master to the slave and wire it to a table window.
local function connect_master(view)
    local master = ModbusMaster {
        device = TcpModbusDevice { host = "127.0.0.1", port = PORT },
        slave_id = 1,
        poll_rate = 200,
        registers = registers,
    }
    -- polled changes -> table (nested under the table's objectName)
    pipe(master, wrap("regs"), view)
    -- table edits -> write to the slave
    pipe(view, master) -- TODO: Should unwrap here. QML binding is kinda broken
end

connect_master(make_window("Master A"))
connect_master(make_window("Master B"))
