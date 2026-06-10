-- Smoke test: constructs every worker that runs without external hardware or
-- services, and verifies live roundtrips where possible. Self-checking:
-- exits 0 with "Smoke test OK" on success, exits 1 on timeout/failure.
--
-- Run with:
--   build/bin/radapter tests/smoke.lua

local os = require "os"

local checks = {
    pipe = true,
    ws_roundtrip = true,
    ws_binary_roundtrip = true,
    sql_roundtrip = true,
    service = true,
    async_unhandled = true,
}

local function pass(name)
    if not checks[name] then return end
    checks[name] = nil
    log.info("PASS: {}", name)
    if next(checks) == nil then
        log.info("Smoke test OK")
        shutdown()
    end
end

after(5000, function()
    local missing = {}
    for k in pairs(checks) do missing[#missing + 1] = k end
    log.error("Smoke test FAILED, missing: {}", missing)
    os.exit(1)
end)

-- Test workers: auto names, custom name + category, duplicate detection
local t1 = TestWorker { delay = 200 }
local t2 = TestWorker { delay = 200 }
local t3 = TestWorker { name = "custom", category = "custom_cat", delay = 200 }

assert(t1.name == "test", "auto name: got " .. tostring(t1.name))
assert(t1.origin:find("smoke%.lua:%d+"), "creation origin: got " .. tostring(t1.origin))
assert(t2.name == "test#2", "auto name must get unique suffix: got " .. tostring(t2.name))
assert(t3.name == "custom", "explicit name: got " .. tostring(t3.name))

assert(not pcall(function() return TestWorker { name = "custom" } end),
    "duplicate worker name must fail")

pipe(t1, function(msg)
    pass("pipe")
end)

-- Websocket pair: plain json
local PORT = 17654
local server = WebsocketServer { port = PORT }
local client = WebsocketClient { url = "ws://127.0.0.1:" .. PORT }

pipe(server, function(msg)
    if msg.hello == "smoke" then
        server { reply = "smoke" }
    end
end)
pipe(client, function(msg)
    if msg.reply == "smoke" then
        pass("ws_roundtrip")
    end
end)
pipe(client.events, function(ev)
    if ev.state == "ConnectedState" then
        client { hello = "smoke" }
    end
end)

-- Websocket pair: msgpack + zlib
local server2 = WebsocketServer { port = PORT + 1, protocol = "msgpack", compression = "zlib" }
local client2 = WebsocketClient { url = "ws://127.0.0.1:" .. (PORT + 1), protocol = "msgpack", compression = "zlib" }

pipe(server2, function(msg)
    if msg.hello == "smoke" then
        server2 { reply = "smoke", arr = { 1, 2, 3 } }
    end
end)
pipe(client2, function(msg)
    if msg.reply == "smoke" and msg.arr[3] == 3 then
        pass("ws_binary_roundtrip")
    end
end)
pipe(client2.events, function(ev)
    if ev.state == "ConnectedState" then
        client2 { hello = "smoke" }
    end
end)

-- Sql roundtrip (in-memory sqlite)
local db = Sql { type = "QSQLITE", db = ":memory:" }
db:Exec("CREATE TABLE t (x int)")
db:Exec("INSERT INTO t VALUES (?)", { 42 })
db:Exec("SELECT x FROM t", function(rows, err)
    assert(err == nil, err)
    assert(rows[1][1] == 42, "unexpected select result: " .. __json_encode(rows))
    pass("sql_roundtrip")
end)

-- async: a discarded failing promise must be reported as an error;
-- a subscribed one must reach its callback and NOT be reported
local unhandled_seen = false
local handled_misreported = false
local handled_ok = false

log.set_handler(function(msg)
    local text = tostring(msg.msg)
    if text:find("boom_unhandled") then
        unhandled_seen = true
    end
    if text:find("Unhandled") and text:find("boom_handled") then
        handled_misreported = true
    end
end)

async(function() error("boom_unhandled") end)()

local p = async(function() error("boom_handled") end)()
p(function(res, err)
    assert(err and err:find("boom_handled"), "subscriber must receive the error")
    handled_ok = true
end)

after(300, function()
    log.set_handler(nil)
    assert(handled_ok, "handled async error must reach subscriber")
    assert(not handled_misreported, "handled async error must not be reported as unhandled")
    assert(unhandled_seen, "unhandled async error must be reported")
    pass("async_unhandled")
end)

-- make_service correlation over a pure-Lua echo worker
local echo = create_worker(function(self, msg)
    notify_all(self, msg)
end)
local call = make_service(echo, echo, 2000)
call({ payload = 1 }, function(res, err)
    assert(err == nil, err)
    pass("service")
end)

-- Constructed only (no live counterpart): must not throw, reconnect in background
local cache = RedisCache { hash_key = "smoke", reconnect_timeout = 60000 }
local stream = RedisStream { stream_key = "smoke:stream", reconnect_timeout = 60000 }
local device = TcpModbusDevice {
    host = "127.0.0.1",
    port = 1502,
    name = "smoke_dev",
    reconnect_timeout_ms = 60000,
}
local master = ModbusMaster {
    device = device,
    slave_id = 1,
    registers = {
        holding = {
            ["pump:status"] = { index = 1 },
        },
    },
}
