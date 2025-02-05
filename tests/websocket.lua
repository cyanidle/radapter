local server = WebsocketServer{
    port = 6337,
    protocol = "msgpack",
    compression = "zlib",
}

local client = WebsocketClient{
    url = "ws://127.0.0.1:6337",
    protocol = "msgpack",
    compression = "zlib",
}

pipe(client.events, function (msg)
    log.info("Client event: {}", msg)
end)

pipe(server, function(msg)
    log.info("SERVER <= {}", msg)
end)

pipe(client, function(msg)
    log.info("CLIENT <= {}", msg)
end)


local counter = 0
each(1000, function ()
    counter = counter + 1
    server {data = fmt("From from server #{}", counter), arr = {1, 2, 3}}
end)


each(2000, function ()
    counter = counter + 1
    client {data = fmt("From from client #{}", counter), arr = {1, 2, 3}}
end)
