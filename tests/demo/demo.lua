local com = args[1]
local parse_topics = require "parse_topics"

---@class topics
---@field logs Worker?
---@field led Worker?
local topics = {}

if com then
    local serial = Serial {
        protocol = "msgpack",
        framing = "slip",
        port = com,
        baud = 57600
    }
    
    topics = parse_topics(serial, "./firmware/firmware.ino")

    pipe(topics.log, function (msg)
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

if (topics.led) then
    pipe(view, filter("angle"), function(msg)
        topics.led {
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
