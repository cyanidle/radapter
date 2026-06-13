-- Declarative-config test: builds object graphs from plain tables via the
-- `declare` module, verifies pipes wire up and deliver, a file save/load
-- round-trip, and that `{ ref = ... }` shares a single device between workers.
-- Self-checking: exits 0 with "Declare test OK" on success, 1 on failure.
--
-- Run with:
--   build/bin/radapter tests/declare.lua

local os = require "os"
local declare = require "declare"

local checks = {
    build_pipe = true,
    file_roundtrip = true,
    ref_share = true,
}

local function pass(name)
    if not checks[name] then return end
    checks[name] = nil
    log.info("PASS: {}", name)
    if next(checks) == nil then
        log.info("Declare test OK")
        shutdown()
    end
end

after(5000, function()
    local missing = {}
    for k in pairs(checks) do missing[#missing + 1] = k end
    log.error("Declare test FAILED, missing: {}", missing)
    os.exit(1)
end)

-- 1. Build a graph and verify a declared pipe (with a wrap transform) actually
-- delivers: a TestWorker ticks numbers -> wrapped -> a websocket client ->
-- the server, where we observe the arrival.
local PORT = 17699
local built = declare.build {
    objects = {
        server = { type = "WebsocketServer", config = { port = PORT } },
        client = { type = "WebsocketClient", config = { url = "ws://127.0.0.1:" .. PORT } },
        ticker = { type = "TestWorker", config = { delay = 100 } },
    },
    pipes = {
        { from = "ticker", to = "client", wrap = "tick" },
    },
}

assert(built.server and built.client and built.ticker, "build must return every object")
assert(built.ticker.name == "ticker", "object key must become the worker name")
assert(#built.ticker:get_listeners() == 1, "declared pipe must attach exactly one listener")

pipe(built.server, function(msg)
    if msg.tick ~= nil then
        pass("build_pipe")
    end
end)

-- 2. File save/load round-trip. save_to only serializes (no instantiation), so
-- the objects are created exactly once, here by load_from.
local tmp = os.tmpname()
local rtcfg = {
    objects = {
        rt_a = { type = "TestWorker", config = { delay = 100000 } },
        rt_b = { type = "TestWorker", config = { delay = 100000 } },
    },
    pipes = {
        { from = "rt_a", to = "rt_b", wrap = "v" },
    },
}
declare.save_to { config = rtcfg, path = tmp }
local rebuilt = declare.load_from { path = tmp }
os.remove(tmp)

assert(rebuilt.rt_a and rebuilt.rt_b, "round-trip must rebuild every object")
assert(rebuilt.rt_a.name == "rt_a", "round-trip must preserve object names")
assert(#rebuilt.rt_a:get_listeners() == 1, "round-trip must rebuild the declared pipe")
pass("file_roundtrip")

-- 3. A shared device referenced by two masters. If the ref did not resolve to a
-- real device object, the ModbusMaster config would fail to parse, so a clean
-- build proves resolution + sharing.
local shared = declare.build {
    objects = {
        plc = { type = "TcpModbusDevice", config = { host = "127.0.0.1", port = 15029 } },
        m1 = { type = "ModbusMaster", config = {
            device = { ref = "plc" },
            slave_id = 1,
            registers = { holding = { ["a"] = { index = 1 } } },
        } },
        m2 = { type = "ModbusMaster", config = {
            device = { ref = "plc" },
            slave_id = 2,
            registers = { holding = { ["b"] = { index = 1 } } },
        } },
    },
}
assert(shared.m1 and shared.m2, "both masters must build sharing one device")
pass("ref_share")
