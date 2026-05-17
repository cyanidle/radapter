local a = async

Cyphal {
    can = CAN {
        plugin = "socketcan",
        device = "vcan0"
    },
    node_id = 93,
    services = {
        {
            type = "uavcan.node.ExecuteCommand.1.2",
            port = 400,
            handler = function(req, cb)
                log("Req: {}", req)
                cb({
                    status = 1
                })
            end
        }
    }
}

local node2 = Cyphal {
    can = CAN {
        plugin = "socketcan",
        device = "vcan0"
    },
    node_id = 94,
}

local executeParams = {
    type = "uavcan.node.ExecuteCommand.1.2",
    server = 93,
    port = 400,
}

a.sync(function (state)
    local promise = node2:Request(executeParams, {
        command = 65529,
        parameter = "kekus",
    })
    local res, err = a.wait(promise)
    log("Res: {}. Err: {}", res, err)
end)()


