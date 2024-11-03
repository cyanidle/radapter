local device = TcpModbusDevice {
    host = "localhost",
    port = 1502,
}

local master = ModbusMaster {
    device = device,
    slave_id = 2,
    registers = {
        holding = {
            ["pump:status"] = {
                index = 1,
            },
            ["pump:error"] = {
                index = 2,
            },
            ["pump:speed"] = {
                index = 3,
                type = "float32",
            },
            ["pump:mode"] = {
                index = 7,
            },
        },
    },
    queries = {
        {type = "holding", index = 1, count = 10},
    }
}

-- TODO: args.key from cli!

pipe(master,
function (msg)
    log("Status changed: {}", get(msg, "pump:status"))
    log("Error changed: {}", get(msg, "pump:error"))
    log("Speed changed: {}", get(msg, "pump:speed"))
end)

local mode = 0
each(3000, function()
    mode = mode + 1
    master {
        pump = {
            mode = mode,
        }
    }
end)
