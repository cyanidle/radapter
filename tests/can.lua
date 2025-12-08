local can = CAN {
    plugin = "socketcan",
    device = "vcan0",
}


each(1000, function ()
    can {
        frame_id = 3,
        payload = "\x12"
    }
end)

pipe(can, log.info)