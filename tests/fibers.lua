-- A fiber parked in an await is torn down by the shutdown unwind. The unwind crosses
-- every lua <-> cpp frame in between, but no C++ exception ever travels through the Lua
-- frames: the Lua side unwinds with a catchable "Forced shutdown" error raised at the
-- await, and the fiber entry rethrows the boost forced_unwind it carries. That is
-- runtime-neutral - LuaJIT would otherwise turn the raw C++ unwind into its own
-- "C++ exception" error and simply resume the fiber.
--
-- The chain is: lua (spawn body) -> cpp (spawn entry, which consumes the shutdown at
-- its pcall boundary and rejects the spawn promise) -> lua (swapper) -> cpp
-- (TestWorker:Call) -> lua (inner) -> cpp (await).
-- User pcall/xpcall may catch the shutdown, but the fiber can not park again: a further
-- await re-raises it, and the fiber still unwinds to its entry (the tripwires below are
-- only reachable if the unwind is swallowed).

local cbs = {}
local never = promise(function (cb) cbs[#cbs] = cb end)
-- anchored: these subscriptions must outlive the fiber threads they are parked on
_G.__never_cbs = cbs

local worker = TestWorker { delay = 10000 }

local parked = {}

local function is_shutdown(e)
    return type(e) == "string" and e:find("Forced shutdown", 1, true) ~= nil
end

local function await_forever(name)
    parked[name] = true
    never:await()
end

-- A caught shutdown must not let the fiber park again: the unprotected await at the end
-- re-raises and carries the shutdown out of the calling frame.
local function await_must_rethrow(name, code)
    local ok, err = pcall(function () never:await() end)
    if ok or not is_shutdown(err) then
        log.error("{}: await on an unwinding fiber must rethrow, got {}", name, tostring(err))
        os.exit(code)
    end
    never:await()
    os.exit(code + 10)
end

local function inner(name)
    await_forever(name)
    os.exit(1)
end

local function inner_pcalled(name)
    local ok, err = pcall(await_forever, name)
    if ok or not is_shutdown(err) then
        log.error("pcall: expected forced shutdown, got {}", tostring(err))
        os.exit(2)
    end
    await_must_rethrow("pcall", 30)
end

local function via_worker(fn, name)
    worker:Call(function ()
        fn(name)
    end)
    os.exit(4)
end

local function via_worker_xpcalled(fn, name)
    local ok, err = xpcall(via_worker, debug.traceback, fn, name)
    if ok or not is_shutdown(err) then
        log.error("xpcall: expected forced shutdown, got {}", tostring(err))
        os.exit(5)
    end
    await_must_rethrow("xpcall", 50)
end

spawn(function ()
    log "plain"
    via_worker(inner, "plain")
    os.exit(7)
end)

spawn(function ()
    log "pcall"
    via_worker(inner_pcalled, "pcall")
    os.exit(8)
end)

spawn(function ()
    log "xpcall"
    via_worker_xpcalled(inner, "xpcall")
    os.exit(9)
end)

after(300, function ()
    for _, name in ipairs { "plain", "pcall", "xpcall" } do
        assert(parked[name], "fiber '" .. name .. "' never parked in the await")
    end
    log "all fibers parked, shutting down"
    shutdown(500)
end)
