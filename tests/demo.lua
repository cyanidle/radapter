local view = QML {
    url = "./Demo.qml",
    props = {"angle"}
}

local redis = RedisCache {
    hash_key = "gui"
}

pipe {
    redis,
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
    redis
}
