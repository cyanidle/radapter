-- ModbusSlave <-> ModbusMaster loopback over TCP on localhost.
-- Self-checking: exits 0 on success, 1 on timeout.
--
-- Run with:
--   build/bin/radapter tests/modbus_loopback.lua

local os = require "os"

local PORT = 11502

local checks = {
    slave_to_master = true,
    master_to_slave = true,
}

local function pass(name)
    if not checks[name] then return end
    checks[name] = nil
    log.info("PASS: {}", name)
    if next(checks) == nil then
        log.info("Modbus loopback OK")
        shutdown()
    end
end

after(5000, function()
    local missing = {}
    for k in pairs(checks) do missing[#missing + 1] = k end
    log.error("Modbus loopback FAILED, missing: {}", missing)
    os.exit(1)
end)

local registers = {
    holding = {
        ["to_master"] = { index = 0 },
        ["speed"] = { index = 1, type = "float32" },
        ["to_slave"] = { index = 3 },
    },
    coils = {
        ["flag"] = { index = 0 },
    },
}

local slave = ModbusSlave {
    device = TcpModbusServer {
        host = "0.0.0.0",
        port = PORT,
    },
    slave_id = 1,
    registers = registers,
}

local device = TcpModbusDevice {
    host = "127.0.0.1",
    port = PORT,
}

local master = ModbusMaster {
    device = device,
    slave_id = 1,
    poll_rate = 100,
    registers = registers,
}

-- Slave -> Master: set local registers, master polls them
slave {
    to_master = 7,
    speed = 3.5,
}

pipe(master, function(msg)
    log.info("MASTER <= {}", msg)
    if get(msg, "to_master") == 7 then
        pass("slave_to_master")
    end
end)

-- Master -> Slave: write over the wire, slave reports the change
after(500, function()
    master {
        to_slave = 9,
        flag = true,
    }
end)

local got = {}
pipe(slave, function(msg)
    log.info("SLAVE <= {}", msg)
    for k, v in pairs(msg) do
        got[k] = v
    end
    if got.to_slave == 9 and got.flag == true then
        pass("master_to_slave")
    end
end)
