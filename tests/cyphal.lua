local can = CAN {
    plugin = "socketcan",
    device = "vcan0",
    can_fd = false
}


local node = Cyphal {
    can = can,
    node_id = 93,
}