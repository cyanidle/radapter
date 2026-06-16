-- Schema-driven GUI graph configurator for Modbus / Redis / SQL / Websocket workers.
--
-- The radapter config schema (the same tree `--schema` prints) is exposed to Lua
-- via the global schema(). We hand the relevant worker schemas to a QML window
-- (radapter's reusable WorkerGraph / WorkerConfigurator / SchemaForm components).
-- The window is a small graph editor: add workers, see them on a canvas grouped by
-- type (each with a badge of how many pipes touch it), click one to edit its config,
-- and connect two into a pipe. The authored model (objects + pipes) is shown live as
-- a declare-compatible config — ready for declare.build / declare.save_to.
--
-- Run (needs a Qt GUI):
--   build/bin/radapter --gui projects/scada/configurator.lua

-- worker families this configurator offers (devices included so Modbus masters
-- can reference / create one)
local supported = {
    "ModbusMaster", "ModbusSlave",
    "TcpModbusDevice", "RtuModbusDevice", "TcpModbusServer", "RtuModbusServer",
    "RedisCache", "RedisStream",
    "Sql",
    "WebsocketServer", "WebsocketClient",
}

-- only the worker types the user picks directly (devices are reached via the ref
-- field's "New…" flow, so they need a schema but not a top-level entry)
local pickable = {
    "ModbusMaster", "ModbusSlave",
    "RedisCache", "RedisStream",
    "Sql",
    "WebsocketServer", "WebsocketClient",
}

local all = schema()
local schemas = {}
for _, name in ipairs(supported) do
    schemas[name] = assert(all[name], "missing schema for " .. name)
end

-- Bespoke editors for complex fields can be supplied per "<WorkerType>.<field>" as a
-- QML file loaded by URL (the extension point for richer configurators). The Modbus
-- register/query editors ship with radapter and SchemaForm applies them automatically,
-- so none are needed here; pass extra ones via custom_forms if desired.
local custom_forms = {}

local view = QML {
    url = "./Configurator.qml",
    properties = {
        custom_forms = custom_forms,
    },
}

-- a default set to start from: 2 Modbus masters (each pointing at a device), a
-- slave, the 2 referenced Tcp devices, and a Redis client. The masters reference
-- the devices the same way the GUI's "New device" flow does ({ ref = "<name>" }).
local default_config = {
    objects = {
        master1 = { type = "ModbusMaster", config = { device = { ref = "plc1" } } },
        master2 = { type = "ModbusMaster", config = { device = { ref = "plc2" } } },
        slave1  = { type = "ModbusSlave",  config = {} },
        plc1    = { type = "TcpModbusDevice", config = {} },
        plc2    = { type = "TcpModbusDevice", config = {} },
        cache1  = { type = "RedisCache",   config = {} },
    },
    pipes = {},
}

-- restrict the type picker to the pickable set, and seed the canvas
view {
    schemas = schemas,
    pickable = pickable,
    config = default_config,
}

-- ── Run: launch the authored config in a separate headless adapter ───────────
--
-- We stand up ONE local-IPC server. Pressing "Run" (the GUI sends { run = config })
-- spawns a headless `runner.lua`, passing it the server's socket name and a fresh
-- token. The runner connects back, announces its token ({hello=token}); we match it
-- to the pending config, send the config to that client, and stream its logs back to
-- the GUI's log window. "Stop" (or a re-run) shuts the previous runner down.

local SERVER = "radapter-scada-" .. tostring(__gen_id())
local server = LocalServer { socket = SERVER }

-- this script's directory, to locate the sibling runner.lua for spawning
local here = debug.getinfo(1, "S").source:match("^@(.*[/\\])") or "./"
local RUNNER = here .. "runner.lua"

local pending = {}      -- token -> config awaiting its runner's hello
local active = nil      -- { proc, token, client } for the current run

local function stop_runner()
    if not active then return end
    if active.client then
        server { [active.client] = { shutdown = true } }   -- ask it to quit gracefully
    end
    if active.proc then active.proc:destroy() end          -- terminate the child too
    active = nil
end

local function start_runner(config)
    stop_runner()

    local token = tostring(__gen_id())
    pending[token] = config
    -- app_info().executable is *this* radapter binary (Qt's applicationFilePath)
    local proc = Process {
        program = app_info().executable,
        arguments = { RUNNER, SERVER, token },
    }
    active = { proc = proc, token = token, client = nil }
    view { run_state = "starting" }

    pipe(proc.events, function(ev)
        if ev.finished ~= nil then
            view { run_state = "exited(" .. tostring(ev.finished) .. ")" }
        elseif ev.stderr and active and not active.client then
            -- boot diagnostics, until the socket takes over as the log source
            view { log = { level = "warn", category = "runner.boot", msg = tostring(ev.stderr) } }
        end
    end)
end

pipe(server.events, function(ev)
    if ev.disconnected and active and active.client == ev.disconnected then
        view { run_state = "disconnected" }
    end
end)

-- inbound from runners arrives as { ["<clientId>"] = message }
pipe(server, function(wrapped)
    for id, msg in pairs(wrapped) do
        if msg.hello ~= nil then
            local token = tostring(msg.hello)
            if active and active.token == token then
                active.client = id
                view { run_state = "ConnectedState" }
                local cfg = pending[token]
                pending[token] = nil
                if cfg then server { [id] = { config = cfg } } end
            end
        elseif msg.log ~= nil then
            view { log = msg.log }
        end
    end
end)

pipe(view, function(msg)
    if msg.run then
        start_runner(msg.run)
    elseif msg.stop then
        stop_runner()
    end
end)
