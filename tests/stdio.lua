-- Test STDIO worker roundtrip via ProcessWorker binary mode: parent spawns child
-- radapter with STDIO, sends a message through the process stdin, and verifies the
-- echoed reply arrives back on stdout.

local EXE = app_info().executable

local CHILD = [[
    local s = STDIO{framing="slip", protocol="msgpack"}
    pipe(s, function(msg)
        msg.pong = 2
        return msg
    end, s)
]]

local proc = Process {
    program = EXE,
    arguments = { "-e", CHILD },
    binary = { framing = "slip", protocol = "msgpack" },
}

pipe(proc, function(msg)
    assert(msg.ping == 1, "expected ping=1")
    assert(msg.pong == 2, "expected pong=2 from child echo")
    log.info("STDIO test OK")
    shutdown()
end)

pipe(proc.events, function(ev)
    if ev.started then
        proc { ping = 1 }
    end
end)

after(5000, function()
    error("STDIO test timed out")
end)
