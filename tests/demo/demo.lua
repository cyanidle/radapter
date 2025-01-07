local com = args[1]
local a = require "async"

---@param serial Worker
---@param id number
---@param fields string[]
local function make_msg_topic(serial, id, fields)
    local send = function (msg)
        log.debug("Sending to device: {}", msg)
        serial(msg)
    end
    local function pack_tuple(msg)
        local tuple = {}
        for i, key in ipairs(fields) do
            tuple[i] = msg[key]
        end
        return tuple
    end
    local function unpack_tuple(tup)
        local msg = {}
        for i, key in ipairs(fields) do
            msg[key] = tup[i]
        end
        return msg
    end
    local topic = create_worker(function(self, msg)
        send {id, pack_tuple(msg)}
    end)
    pipe(serial, function (msg)
        local mid = msg[1]
        local mbody = msg[2]
        if mid == id then
            notify_all(topic, unpack_tuple(mbody))
        end
    end)
    return topic
end

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
        log("CMD: {}", msg)
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
