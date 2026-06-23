-- SCADA config runner. Spawned by the configurator (see configurator.lua) as:
--   radapter projects/scada/runner.lua <server-socket-name> <token>
--
-- Connects back to the configurator's single LocalServer, announces its token,
-- receives a declarative { objects, pipes } config, builds it, and streams every
-- log line back to the configurator over the same socket.
--
-- Wire format (over the local socket):
--   configurator -> runner : { config = { objects, pipes }, observe = bool }  -- build it
--                            { shutdown = true }                -- stop the runner
--   runner -> configurator : { hello = token }                 -- on connect (correlation)
--                            { log = { level, msg, category, timestamp } }
--                            { tag = { name, value, quality, ts } }  -- observe mode

local declare = require "declare"   -- sibling module (resolved via the script's dir)

-- this script's directory, to locate the sibling hmi/ QML for the visualization window
local here = debug.getinfo(1, "S").source:match("^@(.*[/\\])") or "./"

local SERVER = assert(args[1], "runner: server socket name required (argument 1)")
local TOKEN  = assert(args[2], "runner: token required (argument 2)")

local client = LocalClient { socket = SERVER }

-- forward every log line to the configurator. The engine suppresses logs emitted
-- *inside* this handler, so sending here cannot recurse.
log.set_handler(function(entry)
    client { log = entry }
end)

local built = nil

pipe(client, function(msg)
    if msg.shutdown then
        log.info("runner: shutdown requested by configurator")
        shutdown()
    elseif msg.config ~= nil then
        if built then
            log.warn("runner: replacing the running config")
        end
        built = declare.build(msg.config)
        local n = 0
        for _ in pairs(built) do n = n + 1 end
        log.info("runner: built {} object(s) from received config", n)

        -- handle the project's operator visualization. In observe mode (launched by the
        -- configurator, which has its own visualization editor) we stream live tag values
        -- back over the socket so they render there. Standalone, we open our own HMI window
        -- bound to radapter.tags (needs --gui --tags).
        local viz = msg.config.visualization
        if viz and viz.root and viz.root.children and #viz.root.children > 0 then
            if msg.observe then
                if tags then
                    pipe(tags.changed, function(ev) client { tag = ev } end)
                    log.info("runner: streaming live tags to the configurator")
                else
                    log.warn("runner: observe requested but --tags not enabled")
                end
            elseif tags then
                QML { url = here .. "hmi/Hmi.qml", properties = { visualization = viz } }
                log.info("runner: opened HMI visualization window")
            else
                log.warn("runner: visualization present but --tags not enabled; skipping HMI")
            end
        end
    end
end)

-- Self-terminate when the configurator goes away. LocalClient reconnects on drop, so
-- without this a runner whose configurator was killed (not shut down gracefully) would
-- live forever and reconnect to the next configurator that reuses the socket name.
local was_connected = false
pipe(client.events, function(ev)
    if ev.state == "ConnectedState" then
        was_connected = true
        client { hello = TOKEN }
    elseif ev.state == "UnconnectedState" and was_connected then
        log.info("runner: configurator disconnected; shutting down")
        shutdown()
    end
end)

log.info("runner: connecting to {} (token {})", SERVER, TOKEN)
