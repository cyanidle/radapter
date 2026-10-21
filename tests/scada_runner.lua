-- Regression: projects/scada's binary-STDIO runner receives its config as a lazy
-- message view, and the config tree is consumed by table-only code (build(),
-- resolver()). The runner must materialize it and build the received config.

local os = require "os"

local EXE = app_info().executable
local src = debug.getinfo(1, "S").source
local dir = src:match("^@(.*)/[^/]+$") or "."
local SCRIPT = dir .. "/../projects/scada/scada.lua"

local proc = Process {
    program = EXE,
    arguments = {SCRIPT, "run-stdin"},
    binary = {framing = "slip", protocol = "msgpack"},
}

local seen = {}

pipe(proc, function(msg)
    local entry = msg.log
    if not entry then
        return
    end
    seen[#seen + 1] = entry.msg
    if entry.msg:find("built 0 object") then
        log.info("scada runner built the received config")
        shutdown()
    elseif entry.level == "E" then
        log.error("scada runner error: {}", entry.msg)
        os.exit(1)
    end
end)

pipe(proc.events, function(ev)
    if ev.started then
        proc {config = {objects = {}}}
    end
end)

after(5000, function()
    log.error("scada runner did not build the config; log: {}", table.concat(seen, " | "))
    os.exit(1)
end)
