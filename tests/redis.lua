local cache = RedisCache {
    hash_key = "value"
}

local stream = RedisStream {
    stream_key = "value:stream"
}

pipe(cache, wrap("wrapped!"), stream)

pipe {
    cache,
    wrap("wrapped 3!"),
    stream,
}

pipe(
    cache, wrap("wrapped 2!"), stream
)

pipe(cache, function(msg)
    log("Msg from cache: {}", msg)
end)

pipe(stream, unwrap("wrapped!"), function(msg)
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



