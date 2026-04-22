local can = CAN {
    plugin = "socketcan",
    device = "vcan0"
}


local node = Cyphal {
    can = can,
    node_id = 93,
}