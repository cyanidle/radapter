local a = async

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
each(1000, a.sync(function()
    count = count + 1
    cache {
        current = count,
        lol = "%s"
    }
    cache:Exec(fmt("SET test {}", count))
    cache:Exec("GET test", function(res, err)
        log("callback version: Result: {}, Error: {}", res, err)
    end)
    local res, err = a.wait(cache:Exec("GET test"));
    log("async version: GET Result: {}, Error: {}", res, err)
    log("async version (err): GET Result: {}, Error: {}", a.wait(cache:Exec("asdGET test")))
end))



