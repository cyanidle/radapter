-- LocalServer/LocalClient round-trip over Qt local IPC, in one process, exercising
-- both the default json framing and msgpack+zlib: a client connects, the server
-- echoes a structured payload back by client id, and the server observes the
-- disconnect when the client is destroyed.

local results = {}   -- label -> { connected, echo, disconnected }

local function run(label, opts)
    local r = { connected = false, echo = false, disconnected = false }
    results[label] = r

    local socket = "radapter-test-" .. label .. "-" .. __gen_id()
    local server = LocalServer { socket = socket, protocol = opts.protocol, compression = opts.compression }
    local client = LocalClient { socket = socket, protocol = opts.protocol, compression = opts.compression,
                                 reconnect_timeout = 100 }

    pipe(server.events, function(ev)
        if ev.connected then r.connected = true end
        if ev.disconnected then r.disconnected = true end
    end)

    -- echo each client's message straight back to that same client (by id)
    pipe(server, function(wrapped)
        for id, payload in pairs(wrapped) do
            return { [id] = payload }
        end
    end, server)

    pipe(client, function(msg)
        -- a structured payload survives the round-trip intact
        if msg.n == 42 and msg.nested and msg.nested.flag == true and msg.list[2] == "b" then
            r.echo = true
        end
    end)

    pipe(client.events, function(ev)
        if ev.state == "ConnectedState" then
            client { n = 42, nested = { flag = true }, list = { "a", "b", "c" } }
            after(300, function() client:destroy() end)
        end
    end)
end

run("json", {})
run("msgpack", { protocol = "msgpack", compression = "zlib" })

after(1500, function()
    for label, r in pairs(results) do
        assert(r.connected, label .. ": server should report a connection")
        assert(r.echo, label .. ": client should receive its echoed structured payload")
        assert(r.disconnected, label .. ": server should report the disconnect")
    end
    log.info("Local IPC test OK")
    shutdown()
end)
