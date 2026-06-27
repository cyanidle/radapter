-- radapter SCADA: schema-driven Modbus / Redis / SQL / Websocket configurator & runner.
--
-- Modes (selected by first arg):
--   config            Configurator GUI (default).
--   debug             Configurator GUI + auto-start runner with the default config.
--   run <config.json> Standalone production runner: reads and builds a config file.
--   test <name>       Golden test runner (expects goldens/<name>.json).
--   run-stdin         Internal: binary-STDIO runner, spawned by the configurator.
--
-- Examples:
--   build/bin/radapter --gui projects/scada/scada.lua config
--   build/bin/radapter --gui projects/scada/scada.lua config ui-no-persist
--   build/bin/radapter --gui projects/scada/scada.lua debug
--   build/bin/radapter --tags projects/scada/scada.lua run my_project.json
--   build/bin/radapter --gui --tags projects/scada/scada.lua run my_project.json
--   build/bin/radapter projects/scada/scada.lua test basic_workflow
--
-- Record a new golden:
--   build/bin/radapter --gui --gui-record projects/scada/goldens/<name>.json \
--       projects/scada/scada.lua config ui-no-persist

-- ═══════════════════════════════════════════════════════════════════════════════
-- Declarative config helpers (inlined from declare.lua)
-- ═══════════════════════════════════════════════════════════════════════════════

local function resolver(instantiate)
    local function resolve(v)
        if type(v) ~= "table" then
            return v
        end
        if type(v.ref) == "string" then
            return instantiate(v.ref)
        end
        if type(v.type) == "string" and type(v.config) == "table"
            and type(_G[v.type]) == "function" then
            local factory = _G[v.type]
            return factory(resolve(v.config))
        end
        local out = {}
        for k, val in pairs(v) do
            out[k] = resolve(val)
        end
        return out
    end
    return resolve
end

local function build(config)
    assert(type(config) == "table", "config table expected")
    local objects = config.objects or {}
    local built = {}
    local building = {}
    local resolve

    local function instantiate(name)
        local existing = built[name]
        if existing ~= nil then
            return existing
        end
        if building[name] then
            error("dependency cycle while building object '" .. name .. "'")
        end
        local entry = objects[name]
        if type(entry) ~= "table" then
            error("unknown object reference: '" .. name .. "'")
        end
        local factory = _G[entry.type]
        if type(factory) ~= "function" then
            error("object '" .. name .. "' has unknown type '" .. tostring(entry.type) .. "'")
        end
        building[name] = true
        local cfg = resolve(entry.config or {})
        cfg.name = cfg.name or name
        local obj = factory(cfg)
        building[name] = nil
        built[name] = obj
        return obj
    end

    resolve = resolver(instantiate)

    for name in pairs(objects) do
        instantiate(name)
    end

    for i, p in ipairs(config.pipes or {}) do
        local from = built[p.from]
        local to = built[p.to]
        assert(from, "pipe #" .. i .. ": unknown source '" .. tostring(p.from) .. "'")
        assert(to, "pipe #" .. i .. ": unknown target '" .. tostring(p.to) .. "'")
        if p.wrap then
            pipe(from, wrap(p.wrap), to)
        elseif p.unwrap then
            pipe(from, unwrap(p.unwrap), to)
        elseif p.on then
            on(from, p.on, to)
        else
            pipe(from, to)
        end
    end

    return built
end

local PIPES_FIELD = "@pipes"
local VIZ_FIELD = "@visualization"

local function read_file(path)
    local f, err = io.open(path, "r")
    if not f then
        error("could not open '" .. path .. "': " .. tostring(err))
    end
    local data = f:read("*a")
    f:close()
    return data
end

local function write_file(path, data)
    local f, err = io.open(path, "w")
    if not f then
        error("could not open '" .. path .. "' for writing: " .. tostring(err))
    end
    f:write(data)
    f:close()
end

local function redis_conn(params)
    return RedisCache {
        name = "config_io",
        host = params.host,
        port = params.port,
        db = params.db,
    }
end

local function save_to(params)
    assert(type(params) == "table", "params table expected")
    local config = params.config
    assert(type(config) == "table", "save_to: 'config' table required")

    if params.path then
        write_file(params.path, json_encode(config))
    end

    if params.key then
        local cache = redis_conn(params)
        local ok, err = pcall(function()
            local fields = {}
            for name, entry in pairs(config.objects or {}) do
                fields[#fields + 1] = name
                fields[#fields + 1] = json_encode(entry)
            end
            fields[#fields + 1] = PIPES_FIELD
            fields[#fields + 1] = json_encode(config.pipes or {})
            if config.visualization then
                fields[#fields + 1] = VIZ_FIELD
                fields[#fields + 1] = json_encode(config.visualization)
            end
            await(cache:Exec("DEL " .. params.key))
            await(cache:Exec("HSET " .. params.key, fields))
        end)
        cache:destroy()
        if not ok then error(err) end
    end

    if not params.path and not params.key then
        error("save_to: provide 'path' (file) and/or 'key' (redis)")
    end
end

local function read(params)
    assert(type(params) == "table", "params table expected")

    if params.path then
        return json_decode(read_file(params.path))
    elseif params.key then
        local cache = redis_conn(params)
        local ok, res = pcall(function()
            local flat = await(cache:Exec("HGETALL " .. params.key))
            local objects = {}
            local pipes = {}
            local visualization = nil
            for i = 1, #flat, 2 do
                local field, value = flat[i], flat[i + 1]
                if field == PIPES_FIELD then
                    pipes = json_decode(value)
                elseif field == VIZ_FIELD then
                    visualization = json_decode(value)
                else
                    objects[field] = json_decode(value)
                end
            end
            return { objects = objects, pipes = pipes, visualization = visualization }
        end)
        cache:destroy()
        if not ok then error(res) end
        return res
    else
        error("read: provide 'path' (file) or 'key' (redis)")
    end
end

local function load_from(params)
    return build(read(params))
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- Shared data: worker families, schemas, default config
-- ═══════════════════════════════════════════════════════════════════════════════

local supported = {
    "ModbusMaster", "ModbusSlave",
    "TcpModbusDevice", "RtuModbusDevice", "TcpModbusServer", "RtuModbusServer",
    "RedisCache", "RedisStream",
    "Sql",
    "WebsocketServer", "WebsocketClient",
}

local pickable = {
    "ModbusMaster", "ModbusSlave",
    "RedisCache", "RedisStream",
    "Sql",
    "WebsocketServer", "WebsocketClient",
}

local function extract_schemas()
    local all = schema()
    local s = {}
    for _, name in ipairs(supported) do
        s[name] = assert(all[name], "missing schema for " .. name)
    end
    return s
end

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

-- ═══════════════════════════════════════════════════════════════════════════════
-- Binary-STDIO runner (internal: spawned by configurator as a child process)
-- ═══════════════════════════════════════════════════════════════════════════════

local function mode_run_stdin()
    local link = STDIO { framing = "slip", protocol = "msgpack" }

    log.set_handler(function(entry)
        link { log = entry }
    end)

    local built = nil

    pipe(link, function(msg)
        if msg.shutdown then
            log.info("runner: shutdown requested by configurator")
            shutdown()
        elseif msg.config ~= nil then
            if built then
                log.warn("runner: replacing the running config")
            end
            built = build(msg.config)
            local n = 0
            for _ in pairs(built) do n = n + 1 end
            log.info("runner: built {} object(s) from received config", n)

            local viz = msg.config.visualization
            if viz and viz.root and viz.root.children and #viz.root.children > 0 then
                if msg.observe then
                    if tags then
                        pipe(tags.changed, function(ev) link { tag = ev } end)
                        log.info("runner: streaming live tags to the configurator")
                    else
                        log.warn("runner: observe requested but --tags not enabled")
                    end
                elseif tags then
                    QML { url = "hmi/Hmi.qml", properties = { visualization = viz } }
                    log.info("runner: opened HMI visualization window")
                else
                    log.warn("runner: visualization present but --tags not enabled; skipping HMI")
                end
            end
        elseif msg.send_to then
            local w = built and built[msg.send_to]
            if w then
                w(msg.msg)
            else
                log.warn("runner: send_to: unknown worker '{}'", msg.send_to)
            end
        end
    end)

    pipe(link.events, function(ev)
        if ev.closed then
            log.info("runner: stdin closed; shutting down")
            shutdown()
        end
    end)

    log.info("runner: ready, waiting for config")
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- Standalone file-based runner: scada.lua run <config.json>
-- ═══════════════════════════════════════════════════════════════════════════════

local function mode_run()
    local path = assert(args[2], "usage: scada.lua run <config.json>")
    local cfg = read({ path = path })
    local built = build(cfg)
    local n = 0
    for _ in pairs(built) do n = n + 1 end
    log.info("scada: built {} object(s) from {}", n, path)

    local viz = cfg.visualization
    if viz and viz.root and viz.root.children and #viz.root.children > 0 then
        if tags then
            QML { url = "hmi/Hmi.qml", properties = { visualization = viz } }
            log.info("scada: opened HMI visualization window")
        else
            log.warn("scada: visualization present but --tags not enabled; skipping HMI")
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- Configurator GUI (config / debug modes)
-- ═══════════════════════════════════════════════════════════════════════════════

local function configurator_gui(autostart_runner)
    local schemas = extract_schemas()
    local custom_forms = {}

    -- UI persistence is on by default; pass "ui-no-persist" as a trailing argument
    -- to disable (golden tests set persistUi=false directly in QML properties).
    local persist_ui = true
    for _, a in ipairs(args) do
        if a == "ui-no-persist" then persist_ui = false; break end
    end

    local view = QML {
        url = "./configurator/Configurator.qml",
        properties = {
            persistUi = persist_ui,
            custom_forms = custom_forms,
            home = os.getenv("HOME") or "/",
            qt_version = app_info().qt_version,
            github_url = "https://github.com/cyanidle/radapter",
        },
    }

    view {
        schemas = schemas,
        pickable = pickable,
        config = default_config,
    }

    -- ── Run: launch the authored config in a separate headless adapter ───────

    local SELF = "scada.lua"
    local active = nil

    local function stop_runner()
        if not active then return end
        active.proc { shutdown = true }
        active.proc:destroy()
        active = nil
    end

    local function has_visualization(config)
        local viz = config.visualization
        return viz and viz.root and viz.root.children and #viz.root.children > 0
    end

    local function start_runner(config)
        stop_runner()

        local observe = has_visualization(config)
        local arguments = {}
        if observe then
            arguments[#arguments + 1] = "--tags"
        end
        arguments[#arguments + 1] = SELF
        arguments[#arguments + 1] = "run-stdin"

        local proc = Process {
            program = app_info().executable,
            arguments = arguments,
            binary = { framing = "slip", protocol = "msgpack" },
        }
        active = { proc = proc, config = config, observe = observe }
        view { run_state = "starting" }

        pipe(proc, function(msg)
            if msg.log ~= nil then
                view { log = msg.log }
            elseif msg.tag ~= nil then
                view { tag = msg.tag }
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

    -- ── File save/load ────────────────────────────────────────────────────

    local current_config = default_config

    pipe(view, function(msg)
        if msg.save_file then
            local path = msg.save_file
            if msg.config then current_config = msg.config end
            local ok, err = pcall(save_to, { path = path, config = current_config })
            if ok then
                view { save_ok = path }
                log.info("Saved project to {}", path)
            else
                view { save_err = path, save_msg = tostring(err) }
                log.error("Failed to save {}: {}", path, err)
            end
        elseif msg.open_file then
            local path = msg.open_file
            local ok, cfg = pcall(read, { path = path })
            if ok then
                current_config = cfg
                view { config = cfg }
                view { open_ok = path }
                log.info("Loaded project from {}", path)
            else
                view { open_err = path, open_msg = tostring(cfg) }
                log.error("Failed to open {}: {}", path, cfg)
            end
        elseif msg.config then
            current_config = msg.config
        end
    end)

    -- Auto-start runner in debug mode
    if autostart_runner then
        after(200, function()
            start_runner(default_config)
        end)
    end
end

local function mode_config()
    configurator_gui(false)
end

local function mode_debug()
    configurator_gui(true)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- Golden test mode: scada.lua test [record] <name>
-- ═══════════════════════════════════════════════════════════════════════════════

local function mode_test_record()
    local golden_name = assert(args[3], "usage: scada.lua test record <golden_name>")

    local stdio = STDIO { framing = "slip", protocol = "msgpack" }

    local golden_path = "goldens/" .. golden_name .. ".json"
    local golden = assert(json_decode(assert(io.open(golden_path)):read("*a")))

    local replay_events = {}
    for _, ev in ipairs(golden) do
        if ev.type and ev.type ~= "note" then table.insert(replay_events, ev) end
    end

    local schemas = extract_schemas()

    local qt = QML_Tester()
    qt:record_start()

    local view = QML {
        url = "configurator/Configurator.qml",
        properties = {
            persistUi = false,
            home = os.getenv("HOME") or "/",
            qt_version = app_info().qt_version or "5",
            github_url = "https://github.com/cyanidle/radapter",
        },
    }
    qt:wait(1000)
    view { schemas = schemas, pickable = pickable,
        config = { objects = {}, pipes = {},
            visualization = { root = { type = "Column", spacing = 8, children = {} } } } }
    qt:wait(500)

    if #replay_events > 0 then
        local speed = 3.0
        log.info("Replaying {} events at {}x speed...", #replay_events, speed)
        qt:replay_data(json_encode(replay_events), speed)
        qt:wait(1000)
    end

    local events = qt:record_stop()
    stdio(events)
    shutdown()
end

local function mode_test()
    local sub = args[2]
    if sub == "record" then
        mode_test_record()
        return
    end

    local golden_name = assert(sub, "usage: scada.lua test <golden_name>")

    local golden_path = "goldens/" .. golden_name .. ".json"
    local golden_file = io.open(golden_path, "r")
    assert(golden_file, "golden not found: " .. golden_path)
    local golden = assert(json_decode(golden_file:read("*a")))
    golden_file:close()

    local golden_notes = {}
    local golden_events = 0
    for _, ev in ipairs(golden) do
        if ev.type == "note" then
            table.insert(golden_notes, tostring(ev.data))
        elseif ev.type then
            golden_events = golden_events + 1
        end
    end

    log.info("Golden '{}': {} notes, {} replay events",
             golden_name, #golden_notes, golden_events)

    if golden_events == 0 then
        log.error("Golden file has no replayable events — did you record with --gui-record?")
        os.exit(1)
    end

    log.info("Spawning recorder process...")

    local proc = Process {
        program = app_info().executable,
        arguments = { "--gui", "scada.lua", "test", "record", golden_name },
        binary = { framing = "slip", protocol = "msgpack" },
    }

    local done = false
    local exit_code = nil
    local stderr_lines = {}
    local test_events = nil

    pipe(proc, function(msg)
        test_events = msg
    end)

    pipe(proc.events, function(ev)
        if ev.finished then
            done = true
            exit_code = ev.exit_code or ev.signal
        elseif ev.stderr then
            table.insert(stderr_lines, tostring(ev.stderr))
        end
    end)

    local waited = 0
    local timeout_ms = 120000
    local poll_interval = 500

    local function compare_results()
        if exit_code ~= 0 and exit_code ~= "SIGINT" and exit_code ~= "SIGTERM" then
            log.warn("Recorder exited with {}", exit_code)
            for _, line in ipairs(stderr_lines) do
                log.warn("  stderr: {}", line)
            end
        end

        if not test_events then
            log.error("No events received from recorder process")
            os.exit(1)
        end

        local test_notes = {}
        for _, ev in ipairs(test_events) do
            if ev.type == "note" then
                table.insert(test_notes, tostring(ev.data))
            end
        end

        log.info("Test output: {} notes captured", #test_notes)

        local match = true
        local max_n = math.max(#golden_notes, #test_notes)

        if #golden_notes ~= #test_notes then
            log.error("FAIL: note count mismatch: golden={}, test={}",
                      #golden_notes, #test_notes)
            match = false
        end

        for i = 1, max_n do
            local g = golden_notes[i]
            local t = test_notes[i]
            if g == t then
            elseif g == nil then
                log.error("  line {}: unexpected extra note '{}'", i, t)
                match = false
            elseif t == nil then
                log.error("  line {}: missing expected note '{}'", i, g)
                match = false
            else
                log.error("  line {}: expected '{}'", i, g)
                log.error("  line {}:      got '{}'", i, t)
                match = false
            end
        end

        if match then
            log.info("PASS: {} notes match the golden", #golden_notes)
            os.exit(0)
        else
            log.error("FAIL: notes do not match. Update goldens if the changes are intentional:")
            log.error("  --gui --gui-record {} projects/scada/scada.lua config ui-no-persist", golden_path)
            os.exit(1)
        end
    end

    local function poll()
        if done then
            compare_results()
        elseif waited >= timeout_ms then
            log.error("Recorder process timed out after {} ms", timeout_ms)
            proc:destroy()
            os.exit(1)
        else
            waited = waited + poll_interval
            after(poll_interval, poll)
        end
    end

    after(0, poll)
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- Dispatch
-- ═══════════════════════════════════════════════════════════════════════════════

local mode = args[1] or "config"

if mode == "config" then
    mode_config()
elseif mode == "debug" then
    mode_debug()
elseif mode == "run" then
    mode_run()
elseif mode == "run-stdin" then
    mode_run_stdin()
elseif mode == "test" then
    mode_test()
else
    error("unknown mode '" .. mode .. "'. Usage: scada.lua [config|debug|run <path>|test <name>]")
end
