-- Golden test runner for SCADA UI. Two modes:
--
--   Runner mode (headless):
--     build/bin/radapter projects/scada/goldens/run_test.lua <golden_name>
--   Reads the golden, spawns itself with --gui in recorder mode, receives the
--   recorded events via binary stdout (msgpack+SLIP), and compares notes.
--
--   Recorder mode (GUI):
--     build/bin/radapter --gui projects/scada/goldens/run_test.lua record <golden_name>
--   Opens the configurator, replays the golden events, records via QML_Tester,
--   and sends the events back via STDIO (msgpack+SLIP on stdout).
--
-- Record a golden first:
--   build/bin/radapter --gui --gui-record projects/scada/goldens/<name>.json \
--       projects/scada/configurator.lua
--   (interact with the UI; close the window to save)

local mode = args[1]

-- ═══════════════════════════════════════════════════════════════════════════════
-- Recorder mode: replay a golden under --gui and send recorded events via STDIO
-- ═══════════════════════════════════════════════════════════════════════════════
if mode == "record" then
    local golden_name = assert(args[2], "usage: run_test.lua record <golden_name>")

    -- Binary communication channel to the parent process
    local stdio = STDIO{framing="slip", protocol="msgpack"}

    local golden_path = golden_name .. ".json"
    local golden = assert(json_decode(assert(io.open(golden_path)):read("*a")))

    local replay_events = {}
    for _, ev in ipairs(golden) do
        if ev.type and ev.type ~= "note" then table.insert(replay_events, ev) end
    end

    local supported = {"ModbusMaster","ModbusSlave","TcpModbusDevice","RtuModbusDevice",
        "TcpModbusServer","RtuModbusServer","RedisCache","RedisStream","Sql",
        "WebsocketServer","WebsocketClient"}
    local pickable = {"ModbusMaster","ModbusSlave","RedisCache","RedisStream","Sql",
        "WebsocketServer","WebsocketClient"}
    local all = schema()
    local schemas = {}
    for _, n in ipairs(supported) do schemas[n] = assert(all[n]) end

    local qt = QML_Tester()
    qt:record_start()

    local view = QML {
        url = "../Configurator.qml",
        properties = {
            -- Disable QSettings persistence during golden tests: Qt.labs.settings'
            -- file I/O during QML component construction interferes with the
            -- QML_Tester event recording/replay, causing note mismatches.
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

    local events = qt:record_stop()   -- returns Lua table (QVariantList)
    stdio(events)                      -- send via binary stdout to parent
    shutdown()
end

-- ═══════════════════════════════════════════════════════════════════════════════
-- Runner mode (default): spawn recorder with binary Process, compare notes
-- ═══════════════════════════════════════════════════════════════════════════════
local golden_name = assert(mode, "usage: run_test.lua <golden_name>")
if mode == "record" then return end  -- unreachable, handled above

local golden_path = golden_name .. ".json"
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

-- Spawn ourselves in recorder mode under --gui with binary stdout communication.
-- EvalFile sets cwd to this script's directory; the child Process inherits that
-- cwd, so "run_test.lua" resolves as a sibling.
log.info("Spawning recorder process...")

local proc = Process {
    program = app_info().executable,
    arguments = { "--gui", "run_test.lua", "record", golden_name },
    binary = { framing = "slip", protocol = "msgpack" },
}

local done = false
local exit_code = nil
local stderr_lines = {}
-- In binary mode the data channel delivers the deserialized object directly
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

-- Poll until the child exits, then compare
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
        log.error("  --gui --gui-record {} projects/scada/configurator.lua", golden_path)
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
