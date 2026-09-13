-- Forced shutdown unwinds a fiber parked on a promise that is never settled. While
-- that fiber is being torn down its promise dies, which rejects the spawn promise and
-- runs the subscriber below *inside the teardown*. Work started from there must not
-- run: the pool executes entries inline on whichever fiber is current, and during the
-- unwind that fiber is the dying one. The cascade below chains promises through
-- continuations, each level creating the next (deeper than two levels); any level
-- reached once the teardown started trips os.exit(9).

local cbs = {}
-- anchored: the executors must outlive the fibers parked on them
_G.__never_cbs = cbs
local never = promise(function (cb) cbs[#cbs + 1] = cb end)

local teardown = false
local ran = 0

-- Each level creates a promise whose body and continuation create the next level.
local function cascade(n)
    if teardown then
        os.exit(9)  -- tripwire: new work ran during the forced shutdown
    end
    ran = ran + 1
    if n == 0 then
        return
    end
    local p = spawn(function () cascade(n - 1) end)
    p(function (res, err) end)
end

-- sanity check: the same cascade must run to completion normally (7 calls: 6..0)
cascade(6)

local parked = false
spawn(function ()
    parked = true
    never:await()
    os.exit(1)  -- tripwire: a parked fiber must not resume once the shutdown starts
end)(function (res, err)
    -- runs while the fiber above is being unwound - must be refused
    cascade(6)
end)

after(1500, function ()
    if not parked or ran < 7 then
        log.error("cascade/park setup incomplete: parked={} ran={}", parked, ran)
        os.exit(2)
    end
    log "cascade parked, forcing shutdown"
    teardown = true
    shutdown(300)
end)
