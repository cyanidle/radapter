local async_sleep = promisify(function (time, callback)
    after(time, callback)
end)

local echo_in = promisify(function (time, arg, callback)
    after(time, function ()
        log ("Timer callback with: {}", arg)
        callback(arg)
    end)
end)

local async_simple = async(function (arg1, arg2)
    log "Test Start!"

    log("ARGS: {}", {arg1, arg2})

    local A = await(echo_in(1000, 123))
    log "A calculated!"

    local B = await(echo_in(3000, 321))
    log "B calculated!"

    log("a: {}, b: {}", A, B)

    await(async_sleep(1000))

    return A + B
end)

local entry = async(function ()
    local res, err = await(async_simple(23, 25))
    log("RES: {} ERR: {}", res, err)
end)

entry()(function (res, err)
    log "Entry done!"
    shutdown()
end)