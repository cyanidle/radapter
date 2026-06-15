-- Remote config runner. Hosts a WebSocket server, accepts a declarative config
-- (the same { objects, pipes } table the `declare` module builds) from a peer,
-- builds it, and streams every log line back to that peer over the same socket.
--
-- The GUI configurator's "Run" button spawns a headless radapter that calls
-- runner.serve{} and connects to it; the configurator sends the authored config
-- and shows the runner's logs in a separate window. See examples/configurator.
--
-- Wire format (JSON over the socket):
--   peer -> runner : { config = { objects = {...}, pipes = {...} } }  -- build it
--                    { shutdown = true }                              -- stop the runner
--   runner -> peer : { log = { level, msg, category, timestamp } }   -- one log line

local declare = require "declare"

local M = {}

---@param opts { port: integer, name: string? }
---@return Worker server
function M.serve(opts)
    opts = opts or {}
    local port = assert(tonumber(opts.port), "runner.serve: numeric 'port' required")
    local server = WebsocketServer { name = opts.name or "runner", port = port }

    local built = nil

    -- forward every log line to the connected peer(s). The engine suppresses logs
    -- emitted *inside* the handler, so sending here cannot recurse into itself.
    log.set_handler(function(entry)
        server { log = entry }
    end)

    pipe(server, function(msg)
        if msg.shutdown then
            log.info("runner: shutdown requested by peer")
            shutdown()
        elseif msg.config ~= nil then
            if built then
                log.warn("runner: replacing the running config")
            end
            built = declare.build(msg.config)
            local n = 0
            for _ in pairs(built) do n = n + 1 end
            log.info("runner: built {} object(s) from received config", n)
        end
    end)

    log.info("runner: listening on ws://0.0.0.0:{}", port)
    return server
end

return M
