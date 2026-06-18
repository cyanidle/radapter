-- SCADA config runner. Spawned by the configurator (see configurator.lua) as:
--   radapter projects/scada/runner.lua <server-socket-name> <token>
--
-- Connects back to the configurator's single LocalServer, announces its token,
-- receives a declarative { objects, pipes } config, builds it, and streams every
-- log line back to the configurator over the same socket.
--
-- Wire format (over the local socket):
--   configurator -> runner : { config = { objects, pipes } }   -- build it
--                            { shutdown = true }                -- stop the runner
--   runner -> configurator : { hello = token }                 -- on connect (correlation)
--                            { log = { level, msg, category, timestamp } }

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

        -- if the project carries an operator visualization, open the HMI window. The
        -- runner was launched with --gui --tags (see configurator.lua), so the QML worker
        -- has an engine and radapter.tags is live for the widgets to bind to.
        local viz = msg.config.visualization
        if viz and viz.root and viz.root.children and #viz.root.children > 0 then
            if tags then
                QML { url = here .. "hmi/Hmi.qml", properties = { visualization = viz } }
                log.info("runner: opened HMI visualization window")
            else
                log.warn("runner: visualization present but --tags not enabled; skipping HMI")
            end
        end
    end
end)

pipe(client.events, function(ev)
    if ev.state == "ConnectedState" then
        client { hello = TOKEN }
    end
end)

log.info("runner: connecting to {} (token {})", SERVER, TOKEN)
