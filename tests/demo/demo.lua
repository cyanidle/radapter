local com = args[1]
local make_msg_topic = require "make_topic"
local led

if com then
    local serial = Serial {
        protocol = "msgpack",
        framing = "slip",
        port = com,
        baud = 57600
    }
    
    local logs = make_msg_topic(serial, 0, {"data"})
    led = make_msg_topic(serial, 1, {"power"})

    pipe(logs, function (msg)
        log("From device: {}", msg.data)
    end)
end

local view = QML {
    url = "./Demo.qml",
    props = {"angle"}
}

local commands = RedisCache {
    hash_key = "gui"
}

local state = RedisCache {
    hash_key = "gui:state"
}

if (led) then
    pipe(view, filter("angle"), function(msg)
        led {
            power = math.floor(msg.angle)
        }
    end)
end

pipe (
    commands,
    function(msg)
        log("REDIS CMD: {}", msg)
        return msg
    end,
    view
)


pipe (
    view,
    function(msg)
        log("FROM GUI: {}", msg)
        return msg
    end,
    state
)
