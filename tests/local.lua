-- LocalServer/LocalClient round-trips over Qt local IPC, in one process. Covers
-- both server modes and both wire protocols:
--   * per_client=true + json  : keyed routing — server echoes { [id] = payload }
--   * per_client=false + msgpack+zlib : broadcast — server echoes the raw payload
-- A structured payload must survive the round-trip; the server must observe the
-- connect and the disconnect (on client :destroy()).

local results = {}   -- label -> { connected, echo, disconnected }

local function run(label, opts)
    local r = { connected = false, echo = false, disconnected = false }
    results[label] = r

    local socket = "radapter-test-" .. label .. "-" .. next_id()
    local server = LocalServer { socket = socket, per_client = opts.per_client,
                                 protocol = opts.protocol, compression = opts.compression }
    local client = LocalClient { socket = socket, protocol = opts.protocol,
                                 compression = opts.compression, reconnect_timeout = 100 }

    pipe(server.events, function(ev)
        if ev.connected then r.connected = true end
        if ev.disconnected then r.disconnected = true end
    end)

    if opts.per_client then
        -- keyed: echo straight back to the sending client by id
        pipe(server, function(wrapped)
            for id, payload in pairs(wrapped) do return { [id] = payload } end
        end, server)
    else
        -- broadcast: echo the raw payload back to every client
        pipe(server, function(payload) return payload end, server)
    end

    pipe(client, function(msg)
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

run("perclient_json", { per_client = true })
run("broadcast_msgpack", { per_client = false, protocol = "msgpack", compression = "zlib" })

after(1500, function()
    for label, r in pairs(results) do
        assert(r.connected, label .. ": server should report a connection")
        assert(r.echo, label .. ": client should receive its echoed structured payload")
        assert(r.disconnected, label .. ": server should report the disconnect")
    end
    log.info("Local IPC test OK")
    shutdown()
end)
