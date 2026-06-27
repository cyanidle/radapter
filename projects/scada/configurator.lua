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
        home = os.getenv("HOME") or "/",   -- starting dir for the file pickers
        qt_version = app_info().qt_version,
        github_url = "https://github.com/cyanidle/radapter",
    },
}

-- Default topology: one TCP Modbus slave (port 5020) with two masters reading it,
-- each forwarding its data to a Redis stream under a distinct key.
-- All required fields are pre-filled so the Run button is enabled immediately.
local default_config = {
    objects = {
        srv1    = { type = "TcpModbusServer", config = { host = "0.0.0.0", port = 5020 } },
        slave1  = { type = "ModbusSlave",  config = {
            device = { ref = "srv1" }, slave_id = 1,
            registers = { holding = { status = { index = 0 }, setpoint = { index = 1 } } },
        } },
        dev1    = { type = "TcpModbusDevice", config = { host = "127.0.0.1", port = 5020 } },
        dev2    = { type = "TcpModbusDevice", config = { host = "127.0.0.1", port = 5020 } },
        master1 = { type = "ModbusMaster", config = {
            device = { ref = "dev1" }, slave_id = 1,
            registers = { holding = { status = { index = 0 } } },
        } },
        master2 = { type = "ModbusMaster", config = {
            device = { ref = "dev2" }, slave_id = 1,
            registers = { holding = { setpoint = { index = 1 } } },
        } },
        stream1 = { type = "RedisStream",     config = { stream_key = "modbus:master1" } },
        stream2 = { type = "RedisStream",     config = { stream_key = "modbus:master2" } },
    },
    pipes = {
        { from = "master1", to = "stream1" },
        { from = "master2", to = "stream2" },
    },
}

-- restrict the type picker to the pickable set, and seed the canvas. The visualization
-- editor derives its candidate tags live from the authored config (see hmi/HmiEditor.qml).
view {
    schemas = schemas,
    pickable = pickable,
    config = default_config,
}

-- ── Run: launch the authored config in a separate headless adapter ───────────
--
-- Pressing "Run" (the GUI sends { run = config }) spawns a headless `runner.lua` as a
-- child Process in binary mode; we talk to it over the child's stdin/stdout
-- (msgpack+SLIP). Once the child is up we push it the config and stream its logs (and,
-- for a project with a visualization, its live tag values) back into the GUI's log/overlay.
-- "Stop" (or a re-run) shuts the previous runner down; the child also self-terminates if
-- this configurator dies, since that closes its stdin.

-- this script's directory, to locate the sibling runner.lua for spawning
local here = debug.getinfo(1, "S").source:match("^@(.*[/\\])") or "./"
local RUNNER = here .. "runner.lua"

local active = nil      -- { proc, config, observe } for the current run

local function stop_runner()
    if not active then return end
    active.proc { shutdown = true }   -- ask it to quit gracefully
    active.proc:destroy()             -- and terminate the child
    active = nil
end

-- does this config carry a non-empty operator visualization?
local function has_visualization(config)
    local viz = config.visualization
    return viz and viz.root and viz.root.children and #viz.root.children > 0
end

local function start_runner(config)
    stop_runner()

    local observe = has_visualization(config)
    -- when the project carries a visualization we run the adapter with --tags and have it
    -- stream live tag values back here (observe mode), so the configurator's visualization
    -- editor shows the running state instead of the runner popping its own HMI window
    local arguments = {}
    if observe then
        arguments[#arguments + 1] = "--tags"
    end
    arguments[#arguments + 1] = RUNNER
    -- app_info().executable is *this* radapter binary (Qt's applicationFilePath)
    local proc = Process {
        program = app_info().executable,
        arguments = arguments,
        binary = { framing = "slip", protocol = "msgpack" },
    }
    active = { proc = proc, config = config, observe = observe }
    view { run_state = "starting" }

    -- runner -> configurator: log lines and (observe mode) live tag values
    pipe(proc, function(msg)
        if msg.log ~= nil then
            view { log = msg.log }
        elseif msg.tag ~= nil then
            view { tag = msg.tag }   -- live tag update for the visualization editor
        end
    end)

    pipe(proc.events, function(ev)
        if ev.started then
            view { run_state = "running" }
            proc { config = active.config, observe = active.observe }
        elseif ev.finished then
            local how = ev.signal and "signal" or ("code " .. tostring(ev.exit_code))
            view { run_state = "exited(" .. how .. ")" }
        elseif ev.stderr then
            -- engine boot diagnostics (the runner routes its own logs over stdout)
            view { log = { level = "warn", category = "runner.boot", msg = tostring(ev.stderr) } }
        end
    end)
end

pipe(view, function(msg)
    if msg.run then
        start_runner(msg.run)
    elseif msg.stop then
        stop_runner()
    elseif msg.send_to and active then
        active.proc { send_to = msg.send_to, msg = msg.msg }
    end
end)

-- ── File save/load (triggered by QML menubar actions) ────────────────────
--
-- QML sends { save_file = path } or { open_file = path } through the view
-- worker. We use Lua's standard io library to read/write project JSON.
-- We keep the current project state in a local variable so we can save it.

local current_config = default_config   -- the live authoring state

pipe(view, function(msg)
    if msg.save_file then
        local path = msg.save_file
        -- the save message carries the GUI's live config; keep our copy in sync
        if msg.config then current_config = msg.config end
        local data = json_encode(current_config)
        local f, err = io.open(path, "w")
        if f then
            f:write(data)
            f:close()
            view { save_ok = path }
            log.info("Saved project to {}", path)
        else
            view { save_err = path, save_msg = err }
            log.error("Failed to save {}: {}", path, err)
        end
    elseif msg.open_file then
        local path = msg.open_file
        local f, err = io.open(path, "r")
        if f then
            local data = f:read("*a")
            f:close()
            local ok, cfg = pcall(json_decode, data)
            if ok then
                current_config = cfg
                view { config = cfg }
                view { open_ok = path }
                log.info("Loaded project from {}", path)
            else
                view { open_err = path, open_msg = "Invalid JSON: " .. tostring(cfg) }
                log.error("Failed to parse {}: {}", path, cfg)
            end
        else
            view { open_err = path, open_msg = err }
            log.error("Failed to open {}: {}", path, err)
        end
    elseif msg.config then
        -- QML pushes the live config alongside other actions; keep our copy for saves
        current_config = msg.config
    end
end)
