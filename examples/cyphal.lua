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
            handler = function(req)
                log("Req: {}", req)
                return {
                    status = 1
                }
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

spawn(function (x, y, z)
    local res, err = node2:Request(executeParams, {
        command = 65529,
        parameter = "kekus",
    }):await()
    log("Res: {}. Err: {}", res, err)
end, 1, 2, 3)


