-- LocalServer/LocalClient round-trip over Qt local IPC, in one process:
-- a client connects, the server echoes its message back by client id, and the
-- server observes the disconnect when the client is destroyed.

local NAME = "radapter-test-" .. __gen_id()

local server = LocalServer { socket = NAME }
local client = LocalClient { socket = NAME, reconnect_timeout = 100 }

local connected_id = nil
local got_echo = false
local disconnected = false

pipe(server.events, function(ev)
    if ev.connected then connected_id = ev.connected end
    if ev.disconnected then disconnected = true end
end)

-- echo each client's message straight back to that same client (by id)
pipe(server, function(wrapped)
    for id, payload in pairs(wrapped) do
        return { [id] = payload }
    end
end, server)

pipe(client, function(msg)
    if msg.ping == "hello" then got_echo = true end
end)

pipe(client.events, function(ev)
    if ev.state == "ConnectedState" then
        client { ping = "hello" }
    end
end)

after(1000, function()
    assert(connected_id, "server should report a connection")
    assert(got_echo, "client should receive its echoed message")
    client:destroy()
    after(400, function()
        assert(disconnected, "server should report the disconnect")
        log.info("Local IPC test OK")
        shutdown()
    end)
end)
