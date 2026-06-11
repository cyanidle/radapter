-- Self-checking test for the tag system (run with: radapter --tags tests/tags.lua)
-- Requires no external hardware or services.

if not tags then
    log.error("tags API not available -- run with --tags flag")
    os.exit(1)
end

local PORT = 17888

local checks = { subscribe = true, changed = true, source = true, quality_get = true }
local function pass(name)
    if not checks[name] then return end
    checks[name] = nil
    log.info("PASS: {}", name)
    if next(checks) == nil then
        log.info("tags test OK")
        shutdown()
    end
end

after(3000, function()
    local missing = {}
    for k in pairs(checks) do missing[#missing+1] = k end
    log.error("FAIL, missing checks: {}", table.concat(missing, ", "))
    os.exit(1)
end)

local server = WebsocketServer { port = PORT, name = "ws.srv" }
local client = WebsocketClient { url = "ws://127.0.0.1:" .. PORT, name = "ws.cli" }

-- Subscribe to a specific tag before it has a value
tags:subscribe("ws.cli:hello", function(ev)
    if ev.value == "world" and ev.quality == "good" then
        pass("subscribe")
    end
end)

-- pipe(tags.changed) fires for any tag update
pipe(tags.changed, function(ev)
    if ev.name == "ws.srv:hello" and ev.value == "world" then
        pass("changed")
    end
end)

pipe(client, function(msg)
    if msg.hello ~= "world" then return end

    -- source() returns the actual worker object
    local src = tags:source("ws.cli:hello")
    if src and src.name == "ws.cli" then
        pass("source")
    end

    -- tags:get() returns current value + quality
    local t = tags:get("ws.cli:hello")
    if t and t.value == "world" and t.quality == "good" then
        pass("quality_get")
    end
end)

pipe(client.events, function(ev)
    if ev.state == "ConnectedState" then
        server { hello = "world" }
        client { hello = "world" }
    end
end)
