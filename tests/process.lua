-- Process worker: echo test, app_info(), and Signal().

local info = app_info()
assert(type(info.executable) == "string" and #info.executable > 0, "app_info.executable")
assert(type(info.pid) == "number", "app_info.pid")

local ok = { echo = false, signal = false }

local function check_done()
    if ok.echo and ok.signal then
        log.info("Process test OK")
        shutdown()
    end
end

-- Phase 1: echo + stdout capture
do
    local out = {}
    local proc = Process { program = "echo", arguments = { "hello", "radapter" } }

    pipe(proc, function(chunk) out[#out + 1] = chunk.stdout:str() end)

    pipe(proc.events, function(ev)
        if ev.finished then
            local s = table.concat(out)
            assert(s:find("hello radapter"), "stdout mismatch: [" .. s .. "]")
            assert(ev.exit_code == 0, "echo should exit 0, got " .. tostring(ev.exit_code))
            assert(proc:State() == "not_running", "state after exit")
            ok.echo = true
            check_done()
        end
    end)
end

-- Phase 2: send SIGINT to a sleeper, verify signal exit
do
    local sleeper = Process { program = "sleep", arguments = { "10" } }

    pipe(sleeper.events, function(ev)
        if ev.finished then
            assert(ev.signal == true, "sleep should be killed by signal")
            assert(sleeper:State() == "not_running", "state after signal exit")
            ok.signal = true
            check_done()
        end
    end)

    after(200, function() sleeper:Signal("INT") end)
end

after(5000, function() error("process test timed out") end)
