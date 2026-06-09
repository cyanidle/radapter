-- Radapter group chat — client.
--
-- Run with (GUI build required):
--   build/bin/radapter --gui tests/chat/client.lua [ws://host:port]
--
-- On first open a nickname dialog is shown. After joining, the chat window
-- becomes active. Messages are sent to server.lua which relays them to every
-- other connected client.

local URL = args[1] or "ws://127.0.0.1:7654"

local client = WebsocketClient { url = URL }

local view = QML { url = "./client.qml", props = {} }

-- Network → GUI: messages relayed by the server.
pipe(client, function(msg)
    log.info("recv: {}", msg)
    return msg
end, view)

-- GUI → Network: user's typed message.
pipe(view, function(msg)
    log.info("send: {}", msg)
    return msg
end, client)

-- Connection state → GUI title.
pipe(client.events, function(ev)
    return { status = ev.state }
end, view)

log.info("Chat client connecting to {} …", URL)
