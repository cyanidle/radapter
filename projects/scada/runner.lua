-- SCADA config runner. Spawned by the configurator (see configurator.lua) as a child
-- Process in binary mode; the two talk over the child's stdin/stdout (msgpack+SLIP):
--   radapter [--tags] projects/scada/runner.lua
--
-- Receives a declarative { objects, pipes } config, builds it, and streams every log
-- line (and, in observe mode, live tag values) back to the configurator over stdout.
--
-- Wire format (binary msgpack objects over stdio):
--   configurator -> runner : { config = { objects, pipes }, observe = bool }  -- build it
--                            { send_to = name, msg = {...} }   -- send a message to a worker
--                            { shutdown = true }                -- stop the runner
--   runner -> configurator : { log = { level, msg, category, timestamp } }
--                            { tag = { name, value, quality, ts } }  -- observe mode

local declare = require "declare"   -- sibling module (resolved via the script's dir)

-- this script's directory, to locate the sibling hmi/ QML for the visualization window
local here = debug.getinfo(1, "S").source:match("^@(.*[/\\])") or "./"

local link = STDIO { framing = "slip", protocol = "msgpack" }

-- forward every log line to the configurator. The engine suppresses logs emitted
-- *inside* this handler, so sending here cannot recurse.
log.set_handler(function(entry)
    link { log = entry }
end)

local built = nil

pipe(link, function(msg)
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
        -- back over stdout so they render there. Standalone, we open our own HMI window
        -- bound to radapter.tags (needs --gui --tags).
        local viz = msg.config.visualization
        if viz and viz.root and viz.root.children and #viz.root.children > 0 then
            if msg.observe then
                if tags then
                    pipe(tags.changed, function(ev) link { tag = ev } end)
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
    elseif msg.send_to then
        local w = built and built[msg.send_to]
        if w then
            w(msg.msg)
        else
            log.warn("runner: send_to: unknown worker '{}'", msg.send_to)
        end
    end
end)

-- Self-terminate when the configurator goes away. The parent dying closes our stdin,
-- which the STDIO worker reports as a 'closed' event — without this an orphaned runner
-- would linger.
pipe(link.events, function(ev)
    if ev.closed then
        log.info("runner: stdin closed; shutting down")
        shutdown()
    end
end)

log.info("runner: ready, waiting for config")
