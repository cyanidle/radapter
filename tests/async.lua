-- Demo of the promise/fiber API: any function may await directly; spawn()
-- detaches a task onto its own fiber so it runs concurrently with the caller.

local echo_in = promisify(function (time, arg, callback)
    after(time, function ()
        log ("Timer callback with: {}", arg)
        callback(arg)
    end)
end)

-- A plain function: awaits suspend it (and its caller) until settled
local function async_simple (arg1, arg2)
    log "Test Start!"

    log("ARGS: {}", {arg1, arg2})

    local A = echo_in(1000, 123):await()
    log "A calculated!"

    local B = echo_in(3000, 321):await()
    log "B calculated!"

    log("a: {}, b: {}", A, B)

    sleep(1000)

    return A + B
end

-- spawn accepts any callable, like promise() does
local callable = setmetatable({}, { __call = function (_, a) return a * 2 end })
local doubled = spawn(callable, 21):await()
if doubled ~= 42 then
    log.error("spawn: callable target expected 42, got {}", tostring(doubled))
    os.exit(1)
end

-- sleep() validates its argument
do
    local ok, err = pcall(sleep, "100")
    assert(not ok and tostring(err):find("number expected"), "sleep('100') must raise: " .. tostring(err))
    ok, err = pcall(sleep, -1)
    assert(not ok and tostring(err):find("negative"), "sleep(-1) must raise: " .. tostring(err))
end

-- spawn: the promise settles with the result of the call (nil, err) on error
local entry = spawn(async_simple, 23, 25)

-- Subscription form instead of :await()
entry(function (res, err)
    log("RES: {} ERR: {}", res, err)
    if err ~= nil or res ~= 444 then
        log.error("spawn: expected the call result 444, got {}", tostring(res))
        os.exit(1)
    end
    log "Entry done!"
    shutdown()
end)
