local a = async

local can = CAN {
    plugin = "socketcan",
    device = "vcan0"
}


local node1 = Cyphal {
    can = can,
    node_id = 93,
}

node1:Call(function ()
    return 1
end)

node1:Call(a.sync(function ()
    a.sleep(3)
    return 2
end))

node1:Call(a.sync(function ()
    error("KEK!")
    a.sleep(2)
    return 2
end))

local node2 = Cyphal {
    can = can,
    node_id = 94,
}