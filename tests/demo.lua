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

pipe {
    commands,
    function(msg)
        log("CMD: {}", msg)
        return msg
    end,
    view
}


pipe {
    view,
    function(msg)
        log("FROM GUI: {}", msg)
        return msg
    end,
    state
}
