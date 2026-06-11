assert(args[1], "Please provide COM port as first argument")

local serial = Serial {
    protocol = "msgpack",
    framing = "slip",
    port = args[1],
    baud = 57600,
}

pipe(serial, function (msg)
    log.info("Responce from device: {}", msg) 
end)

local id = 0
each(500, function ()
    id = id + 1
    log("Sending msg to device, id: {}", id)

    serial {
        id = id,
        kek = {"chebureck", "да"},
    }
end)