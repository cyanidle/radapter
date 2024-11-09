local a = async

async_sleep = a.wrap(function (time, callback)
    after(time, callback)
end)

echo_in = a.wrap(function (time, arg, callback)
    after(time, function ()
        callback(arg)
    end)
end)

---- THESE two are identical to ^

-- async_sleep = function (time)
--     return function (callback)
--         after(time, callback)
--     end
-- end

-- echo_in = function (time, arg)
--     return function (callback)
--         after(time, function ()
--             callback(arg)
--         end)
--     end
-- end

async_main = a.sync(function ()
    log "Test Start!"

    local A = a.wait(echo_in(1000, 123))
    log "A calculated!"

    local B = a.wait(echo_in(3000, 321))
    log "B calculated!"


    log("a: {}, b: {}", A, B)

    a.wait(async_sleep(1000))

    return A, B
end)

async_main(function(A, B)
    log("Test Done! From main: {}, {}", A, B)
end)
