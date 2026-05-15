local a = async

local can = CAN {
    plugin = "socketcan",
    device = "vcan0"
}


local node1 = Cyphal {
    can = can,
    node_id = 93,
    services = {
        {
            type = "uavcan.node.ExecuteCommand.1.2",
            port = 1000,
            handler = a.sync(function(req)
                log("Req: {}", req)
                return {
                    status = 1
                }
            end)
        }
    }
}

local node2 = Cyphal {
    can = can,
    node_id = 94,
}

local promise = node2:Request(
{
    type = "uavcan.node.ExecuteCommand.1.2",
    server = 93,
    port = 1000,
},
{
    command = 65529,
    parameter = "kekus",
}
)

a.sync(function ()
    local res, err = a.wait(promise)
    log("Res: {}. Err: {}", res, err)
end)()