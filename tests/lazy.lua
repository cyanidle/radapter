-- Lazy message views: worker messages cross into Lua as a userdata view over the
-- retained QVariant. Fields, pairs, ipairs, #, tostring (JSON), get, json_encode,
-- materialize, writes (copy-on-write), set and forwarding must all behave like
-- the plain table -- including after the view has been materialized, and for a
-- message whose root is a list.

local os = require "os"

local socket = "radapter-lazy-" .. next_id()
local server = LocalServer {socket = socket}
local client = LocalClient {socket = socket, reconnect_timeout = 100}

-- The server echoes every payload back, so the client receives a message that
-- crossed C++ -> Lua twice (and `server(msg)` below forwards a view again).
pipe(server, function(payload)
    return payload
end, server)

local function sorted_keys(msg)
    local keys = {}
    for k in pairs(msg) do
        keys[#keys + 1] = k
    end
    table.sort(keys)
    return keys
end

local stage = 1

pipe(client, function(msg)
    if stage == 1 then
        assert(type(msg) == "userdata", "message should be a lazy view, got " .. type(msg))
        assert(msg.n == 42, "field access")
        assert(msg.nested.flag == true, "nested field access")
        assert(msg.list[2] == "b", "list index")
        assert(#msg.list == 2, "# on a lazy list")

        local keys = sorted_keys(msg)
        assert(#keys == 3 and keys[1] == "list" and keys[2] == "n" and keys[3] == "nested",
            "pairs keys: " .. table.concat(keys, ","))
        for k, v in pairs(msg) do
            if k == "nested" then
                assert(v.flag == true, "pairs value should be a lazy child view")
            end
        end

        local values = {}
        for i, v in ipairs(msg.list) do
            values[i] = v
        end
        assert(values[1] == "a" and values[2] == "b", "ipairs over a lazy list")

        local decoded = json_decode(tostring(msg))
        assert(decoded.n == 42 and decoded.nested.flag == true and decoded.list[2] == "b",
            "tostring must serialize the whole message as json")

        local encoded = json_decode(json_encode(msg))
        assert(encoded.n == 42 and encoded.list[2] == "b", "json_encode on a view")

        assert(get(msg, "nested:flag") == true, "get path")
        assert(get(msg, "list:[2]") == "b", "get path with a list index")
        assert(get(msg, "nested").flag == true, "get returns a lazy child")
        assert(get(msg, "missing") == nil, "get missing path")

        -- A derived child view cannot propagate an assignment to the message, so
        -- the write must fail loudly instead of vanishing -- for set() as well,
        -- and it stays read-only after being materialized on its own.
        local ok, err = pcall(function()
            msg.nested.flag = false
        end)
        assert(not ok, "writing a nested view must raise")
        assert(tostring(err):find("nested lazy views are read-only", 1, true),
            "nested write error: " .. tostring(err))
        assert(msg.nested.flag == true, "a rejected nested write changes nothing")
        ok, err = pcall(set, msg.nested, "flag", false)
        assert(not ok, "set on a nested view must raise")
        assert(msg.nested.flag == true, "a rejected set changes nothing")

        local sub = msg.nested
        assert(sub:materialize().flag == true, "a child view can be materialized")
        ok, err = pcall(function()
            sub.flag = false
        end)
        assert(not ok, "materializing a child view must not make it writable")
        assert(sub.flag == true and msg.nested.flag == true, "a rejected child write changes nothing")

        local plain = msg:materialize()
        assert(type(plain) == "table" and plain.nested.flag == true and plain.list[2] == "b",
            "materialize")
        assert(rawequal(plain, msg:materialize()), "materialize is cached on the view")
        assert(#sorted_keys(msg) == 3, "pairs after materialize")
        assert(get(msg, "nested:flag") == true, "get after materialize")
        values = {}
        for i, v in ipairs(msg.list) do
            values[i] = v
        end
        assert(values[2] == "b", "ipairs after materialize")

        -- A write to the message root materializes the message; it is visible to
        -- every later read and to a forwarded copy.
        msg.added = true
        assert(msg.added == true, "root write")
        assert(#sorted_keys(msg) == 4, "pairs after a root write")
        assert(#msg == 0, "# on a map message after a write")
        assert(json_decode(tostring(msg)).added == true, "root write is serialized")

        local changed = set(msg, "extra", 7)
        assert(type(changed) == "table" and changed.extra == 7, "set returns the view's table")
        assert(msg.extra == 7, "set materializes the view in place")
        assert(get(msg, "extra") == 7, "set result is visible to get")

        assert(wrap("nested")(msg).nested.n == 42, "wrap keeps the lazy message")
        assert(unwrap("nested")(wrap("nested")(msg)).nested.flag == true, "wrap/unwrap roundtrip")
        assert(unwrap("nested")(msg).flag == true, "unwrap uses get")

        server(msg) -- toQVar fast path: forward the view unchanged
        stage = 2
    elseif stage == 2 then
        assert(type(msg) == "userdata" and msg.nested.flag == true, "forwarded message")
        assert(msg.extra == 7 and msg.added == true, "forwarded message carries the writes")
        -- a view that refers to itself must fail the conversion, not take the process down
        msg.self = msg
        local ok, err = pcall(tostring, msg)
        assert(not ok, "tostring of a cyclic view must raise")
        assert(tostring(err):find("nests too deeply", 1, true), "cyclic view: " .. tostring(err))
        assert(msg.nested.flag == true, "the message is still readable after a cyclic tostring")
        client {10, 20, 30}
        stage = 3
    elseif stage == 3 then
        assert(type(msg) == "userdata", "list message should be a lazy view, got " .. type(msg))
        assert(msg[2] == 20, "list message index")
        assert(#msg == 3, "# on a list message")
        local values = {}
        for i, v in ipairs(msg) do
            values[i] = v
        end
        assert(#values == 3 and values[1] == 10 and values[3] == 30, "ipairs over a list message")
        local n = 0
        for _, _ in pairs(msg) do
            n = n + 1
        end
        assert(n == 3, "pairs over a list message")
        assert(get(msg, "[2]") == 20, "get on a list message")
        assert(get(msg, "missing") == nil, "get miss on a list message")
        assert(tostring(msg) == "[10,20,30]", "tostring of a list message: " .. tostring(msg))
        msg.tag = "x"
        assert(#msg == 3 and msg[3] == 30, "# and index on a list message after a write")
        server(msg)
        stage = 4
    else
        assert(type(msg) == "userdata" and #msg == 3 and msg[2] == 20, "forwarded list message")
        client:destroy()
        shutdown()
    end
end)

after(5000, function()
    log.error("Lazy view test timed out")
    os.exit(1)
end)

match_msg(client.events, function(ev)
    return ev.state == "ConnectedState"
end):await()
client {n = 42, nested = {flag = true}, list = {"a", "b"}}
