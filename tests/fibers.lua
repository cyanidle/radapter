-- A fiber parked in an await is torn down by the shutdown unwind, and the unwind
-- crosses every lua <-> cpp frame in between: the fiber must not resume. Each frame
-- below hits a tripwire (os.exit) if control ever comes back to it, so a swallowed
-- unwind fails the test instead of silently continuing.
--
-- The chain is: lua (spawn body, itself running under xpcall) -> cpp (spawn entry)
-- -> lua (swapper) -> cpp (TestWorker:Call) -> lua (inner) -> cpp (await). The second
-- fiber wraps the await in a user pcall and the third wraps the whole worker call in
-- an xpcall: neither may keep the fiber alive.

local never = promise(function () end)
local worker = TestWorker { delay = 10000 }

local parked = {}

local function await_forever(name)
    parked[name] = true
    never:await()
end

local function inner(name)
    await_forever(name)
    os.exit(1)
end

local function inner_pcalled(name)
    pcall(await_forever, name)
    os.exit(2)
end

local function via_worker(fn, name)
    worker:Call(function ()
        fn(name)
    end)
    os.exit(4)
end

local function via_worker_xpcalled(fn, name)
    local ok = xpcall(via_worker, debug.traceback, fn, name)
    os.exit(5)
end

spawn(function ()
    via_worker(inner, "plain")
    os.exit(3)
end)

spawn(function ()
    via_worker(inner_pcalled, "pcall")
    os.exit(6)
end)

spawn(function ()
    via_worker_xpcalled(inner, "xpcall")
    os.exit(7)
end)

after(300, function ()
    for _, name in ipairs { "plain", "pcall", "xpcall" } do
        assert(parked[name], "fiber '" .. name .. "' never parked in the await")
    end
    log "all fibers parked, shutting down"
    shutdown(300)
end)
