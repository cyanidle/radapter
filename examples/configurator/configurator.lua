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
--   build/bin/radapter --gui examples/configurator/configurator.lua

local declare = require "declare"

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
-- Pressing "Run" in the GUI sends { run = <config> }. We spawn a headless
-- radapter that hosts runner.serve{} on a fresh port, connect to it, push the
-- config, and forward the runner's streamed logs back to the GUI's log window.
-- "Stop" (or a re-run) shuts the previous runner down.

math.randomseed(os.time())

local runner_proc = nil
local runner_client = nil

local function stop_runner()
    if runner_client then
        runner_client { shutdown = true }   -- ask the runner to quit gracefully
        runner_client:destroy()
        runner_client = nil
    end
    if runner_proc then
        runner_proc:destroy()               -- terminates the child if still alive
        runner_proc = nil
    end
end

local function start_runner(config)
    stop_runner()

    local port = math.random(20000, 60000)
    -- app_info().executable is *this* radapter binary (Qt's applicationFilePath)
    local exe = app_info().executable
    log.info("configurator: launching runner on port {}", port)

    -- headless sibling adapter hosting runner.serve{port}. No shell: args are literal,
    -- so no quoting games. The Process worker terminates it when we :destroy() it.
    runner_proc = Process {
        program = exe,
        arguments = { "-e", "require([[runner]]).serve{port=" .. port .. "}" },
    }

    local connected = false
    pipe(runner_proc.events, function(ev)
        if ev.finished ~= nil then
            view { run_state = "exited(" .. tostring(ev.finished) .. ")" }
        elseif ev.stderr and not connected then
            -- boot diagnostics, until the socket takes over as the log source
            view { log = { level = "warn", category = "runner.boot", msg = tostring(ev.stderr) } }
        end
    end)

    local client = WebsocketClient { url = "ws://127.0.0.1:" .. port, reconnect_timeout = 300 }
    runner_client = client
    pipe(client.events, function(ev)
        view { run_state = ev.state }
        if ev.state == "ConnectedState" then
            connected = true
            client { config = config }
        end
    end)
    pipe(client, function(msg)
        if msg.log then view { log = msg.log } end
    end)
end

pipe(view, function(msg)
    if msg.run then
        start_runner(msg.run)
    elseif msg.stop then
        stop_runner()
    end
end)
