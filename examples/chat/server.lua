-- Radapter group chat — headless relay server.
--
-- Run with:
--   build/bin/radapter examples/chat/server.lua [port]
--
-- Accepts any number of WebSocket clients (client.lua).
-- Every message from a client is relayed to all OTHER connected clients.
-- Incoming messages arrive as { ["addr:port"] = payload }; outgoing targeted
-- sends use the same keyed-map format.

local PORT = tonumber(args[1]) or 7654

local server = WebsocketServer { port = PORT, per_client = true }

local clients = {}   -- addr:port -> true

pipe(server.events, function(ev)
    if ev.connected then
        clients[ev.connected] = true
        log.info("+ connected  {}", ev.connected)
    elseif ev.disconnected then
        clients[ev.disconnected] = nil
        log.info("- disconnected {}", ev.disconnected)
    end
end)

pipe(server, function(wrapped)
    -- wrapped = { ["addr:port"] = { from = "Nick", text = "…" } }
    local sender, payload
    for addr, data in pairs(wrapped) do
        sender, payload = addr, data
        break
    end
    if not payload then return end

    log.info("[{}] {}: {}", sender, payload.from or "?", payload.text or "?")

    -- build a targeted map: every peer except the sender
    local out = {}
    for addr in pairs(clients) do
        if addr ~= sender then
            out[addr] = payload
        end
    end
    if next(out) then return out end
end, server)

log.info("Chat relay listening on ws://0.0.0.0:{}", PORT)
