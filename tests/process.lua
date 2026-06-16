-- Process worker + app_info(): run `echo`, capture its stdout on the data channel,
-- and confirm the finished event fires. Also sanity-checks app_info().

local info = app_info()
assert(type(info.executable) == "string" and #info.executable > 0, "app_info.executable")
assert(type(info.pid) == "number", "app_info.pid")

local out = {}
local proc = Process { program = "echo", arguments = { "hello", "radapter" } }

pipe(proc, function(chunk) out[#out + 1] = tostring(chunk) end)

pipe(proc.events, function(ev)
    if ev.finished ~= nil then
        local s = table.concat(out)
        assert(s:find("hello radapter"), "stdout mismatch: [" .. s .. "]")
        assert(ev.finished == 0, "echo should exit 0, got " .. tostring(ev.finished))
        assert(proc:State() == "not_running", "state after exit")
        log.info("Process test OK")
        shutdown()
    end
end)

after(3000, function() error("process never finished") end)
