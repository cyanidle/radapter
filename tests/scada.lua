-- Headless end-to-end for the SCADA runner: stand up a LocalServer, spawn
-- projects/scada/runner.lua over local IPC (passing the socket name + a token),
-- send it a trivial config, and assert its "built" log streams back.

local SERVER = "radapter-scada-test-" .. tostring(__gen_id())
local TOKEN  = "tok-" .. tostring(__gen_id())
-- cwd is the script's dir (tests/) during top-level execution, and the Process
-- spawns now, so this resolves to projects/scada/runner.lua
local RUNNER = "../projects/scada/runner.lua"

local server = LocalServer { socket = SERVER, per_client = true }

local client_id = nil
local got_built = false

local proc = Process {
    program = app_info().executable,
    arguments = { RUNNER, SERVER, TOKEN },
}

-- inbound from the runner arrives as { ["<clientId>"] = message }
pipe(server, function(wrapped)
    for id, msg in pairs(wrapped) do
        if msg.hello == TOKEN then
            client_id = id
            server { [id] = { config = { objects = {}, pipes = {} } } }
        elseif msg.log and tostring(msg.log.msg):find("built") then
            got_built = true
        end
    end
end)

after(4000, function()
    assert(client_id, "runner should connect and announce its token")
    assert(got_built, "runner should build the config and stream its 'built' log back")
    server { [client_id] = { shutdown = true } }
    proc:destroy()
    log.info("SCADA runner test OK")
    shutdown()
end)
