-- Interactive REPL for the firmware's USART2 bench channel (App/serial_line.cpp).
-- Every line typed on stdin is forwarded verbatim to the port; every line the
-- firmware sends is printed to stdout (radapter's own log stays on stderr), so
-- the REPL can also be driven from a pipe:
--
--   radapter examples/serial/serial_repl.lua /dev/ttyACM0
--   printf 'S\nT0.1\nM10\n' | radapter examples/serial/serial_repl.lua /dev/ttyACM0
--
-- Empty line asks for a status line, ":q" quits (EOF on stdin quits too), and
-- shutdown parks the drive: zero set-points and driver disabled.

assert(args[1], "usage: radapter serial_repl.lua <serial-port>")

local port = Serial {
    framing = "line",
    protocol = "text",
    port = args[1],
    baud = 115200,
}

local console = STDIO {
    framing = "line",
    protocol = "text",
}

on_shutdown(function ()
    port("T0")
    port("X")
end)

local function quit()
    -- The commands already on the wire still deserve an answer before the port
    -- disappears, which matters when stdin is a pipe and hits EOF immediately.
    sleep(300)
    shutdown(500)
end

pipe(port, function (line)
    console(line)
end)

pipe(console.events, function (event)
    if event.closed then
        log.info("stdin closed, quitting")
        quit()
    end
end)

pipe(console, function (line)
    local command = line:match("^%s*(.-)%s*$")
    if command == "" then
        command = "S"
    end
    if command:sub(1, 1) == ":" then
        local name = command:sub(2)
        if name == "q" or name == "quit" then
            quit()
        elseif name == "h" or name == "help" then
            console(":q          quit (parks the drive)")
            console(":h          this list")
            console("<empty>     status line (S)")
            console("anything else goes to the firmware verbatim, ? lists its commands")
        else
            log.warn("unknown REPL command '{}'", command)
        end
        return
    end
    log.info("> {}", command)
    port(command)
end)

port("")
port("S")
