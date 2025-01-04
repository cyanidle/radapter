assert(args[1], "Please provide COM port as first argument")

local serial = Serial {
    protocol = "msgpack",
    framing = "slip",
    port = args[1],
    baud = 57600
}

pipe(serial, log.info)