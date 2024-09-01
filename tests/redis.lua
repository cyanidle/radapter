local cache = RedisCache {
    hash_key = "value"
}

local stream = RedisStream {
    stream_key = "value:stream"
}

cache:pipe(wrap("wrapped!")):pipe(stream)

cache:pipe(function(msg)
    log("Msg from cache: {}", msg)
end)

stream:pipe(unwrap("wrapped!")):pipe(function(msg)
    log("Msg from stream: {}", msg)
end)

local count = 0
each(1000, function ()
    count = count + 1
    cache {
        current = count,
        lol = "%s"
    }
    cache:Execute(fmt("SET test {}", count))
    cache:Execute("GET test", function(res, err)
        log("GET Result: {}, Error: {}", res, err)
    end)
end)
