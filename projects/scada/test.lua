-- Headless end-to-end for the SCADA runner: spawn projects/scada/runner.lua as a child
-- Process in binary mode (msgpack+SLIP over stdio), send it a trivial config, and assert
-- its "built" log streams back.

-- cwd is the script's dir (projects/scada/) during top-level execution, so the runner
-- resolves as a sibling
local RUNNER = "./runner.lua"

local got_built = false

local proc = Process {
    program = app_info().executable,
    arguments = { RUNNER },
    binary = { framing = "slip", protocol = "msgpack" },
}

-- runner -> us: log lines (the runner routes its logs over stdout)
pipe(proc, function(msg)
    if msg.log and tostring(msg.log.msg):find("built") then
        got_built = true
    end
end)

pipe(proc.events, function(ev)
    if ev.started then
        proc { config = { objects = {}, pipes = {} } }
    end
end)

after(4000, function()
    assert(got_built, "runner should build the config and stream its 'built' log back")
    proc { shutdown = true }
    proc:destroy()
    log.info("SCADA runner test OK")
    shutdown()
end)
