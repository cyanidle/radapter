local com = args[1]
local parse_topics = require "parse_topics"
local a = require "async"

local topics = {}
local gamble

if com then
    local serial = Serial {
        protocol = "msgpack",
        framing = "slip",
        port = com,
        baud = 57600
    }
    
    topics = parse_topics(serial, "./firmware/firmware.ino")

    local names = {}
    for name, _ in pairs(topics) do
        names[#names+1] = name
    end

    log.debug("Topics: {}", names)

    pipe(topics.log, function (msg)
        log.info("Device: {}", msg.data)
    end)
    pipe(topics.error, function (msg)
        log.error("DEVICE ERROR: {}", msg.data)
    end)

    gamble = make_service(topics.gamble_req, topics.gamble_resp)
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
    pipe(view, unwrap("angle"), function(msg)
        topics.led {
            power = math.floor(msg)
        }
    end)
    pipe(view, unwrap("gamble_request"), function(msg)
        gamble(msg, function (res, err)
            if err ~= nil then
                log.error("Could not gamble!")
                view {
                    gamble_result = {
                        ok = false,
                    }
                }
            else
                log.info("Gambled ok! Payout: {}", res.amount)
                view {
                    gamble_result = {
                        ok = true,
                        amount = res.amount
                    }
                }
            end
        end)
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
