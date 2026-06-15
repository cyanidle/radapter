-- Exercises the runner transport in a single process: start runner.serve, connect
-- a WebsocketClient to it, send a (trivial) declarative config, and assert the
-- runner's "built …" log is streamed back over the same socket.

local runner = require "runner"

local PORT = 7787

local server = runner.serve { port = PORT }

local client = WebsocketClient { url = "ws://127.0.0.1:" .. PORT, reconnect_timeout = 200 }

local got_built_log = false

pipe(client, function(msg)
    if msg.log and tostring(msg.log.msg):find("built") then
        got_built_log = true
    end
end)

pipe(client.events, function(ev)
    if ev.state == "ConnectedState" then
        client { config = { objects = {}, pipes = {} } }
    end
end)

after(1500, function()
    assert(got_built_log, "runner must build the config and stream its log back")
    log.info("Runner test OK")
    shutdown()
end)
