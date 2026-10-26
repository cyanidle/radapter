-- Simple line protocol: framing = "line" + protocol = "text" deliver every
-- received line to Lua as a string, and send strings out with a trailing "\n".
-- Matches the firmware's USART2 bench channel (cart.lua chassis, App/serial_line.cpp).
assert(args[1], "Please provide COM port as first argument")

local motor = Serial {
    framing = "line",
    protocol = "text",
    port = args[1],
    baud = 115200,
}

local function send(line)
    log.info("> {}", line)
    motor(line)
end

pipe(motor, function (line)
    log.info("< {}", line)
end)

send("?")
send("E1")
send("T0.5")

each(1000, function ()
    send("S")
end)

on_shutdown(function ()
    send("T0")
    send("X")
end)
