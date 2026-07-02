log.set_handler(function(msg)
    print("LUA handler:", msg.msg)
end)

log "Start test"

log.set_handler(nil)

local deep = {
    a = { b = { c = 1 } }
}

assert(set(deep, "extra", 1) == deep)
assert(deep.extra == 1)

assert(set({}, "a:b:[2]", "a").a.b[2] == "a")
assert(set({}, "a:b:[0]", "a").a.b[0] == "a")
assert(set({}, "a:b:[1]", 1).a.b[1] == 1)

assert(get(deep, "a:b:c") == 1)
assert(get(deep, "c:b:c") == nil)

assert(get(deep, "") == deep)
assert(get(deep, ":") == deep)

set(deep, "b:c:a", 3)
assert(get(deep, ":b:c:a") == 3)

assert(get(deep, "a/b/c", "/") == 1)
assert(get(deep, "/a/b/c", "/") == 1)

assert(get(deep, "a123b123c", "123") == 1)

set(deep, "test!a!test", 3, "!")
assert(get(deep, "test!a!test", "!") == 3)

local test = TestWorker {
    delay = 1000
}

local json = json_decode('{"command":"welcome","arguments":{"stopOnEntry":false,"sourceBasePath":"/home/alexej/repos/radapter/tests/demo","directorySeperator":"/"}}')
local back = json_encode(json)

pipe(test, test)

assert(not pcall(pipe), "Empty pipe() should fail")

local cancel = pipe(test)
assert(type(cancel) == "function", "pipe(x) should return a cancel function")

assert(pcall(pipe, function() end), "convert function to callable")

pipe(
    test,
    function()
        return 1
    end,
    function(msg)
        return msg
    end
)

local first_cancel = pipe (
    test,
    function(msg)
        log("Original: {}", msg)
        return msg + 1
    end,
    function(msg)
        log("After incr: {}", msg)
        --return msg + 1
    end,
    function(msg)
        log(msg)
        error("Should not be reachable")
    end
)

assert(type(first_cancel) == "function", "pipe(x, y, z) should return a cancel function")

-- Verify cancellation works: after cancelling, new messages don't reach the listener
local collected = {}
local tmp = create_worker(function(self, msg, sender)
    collected[#collected + 1] = msg
    notify_all(self, msg, sender)
end)
local cc = pipe(tmp, function(msg) collected[#collected + 1] = "via_pipe:" .. tostring(msg) end)
tmp(42)
assert(#collected == 2 and collected[1] == 42 and collected[2] == "via_pipe:42", "pipe should forward messages")
cc()  -- cancel the subscription
tmp(99)
assert(#collected == 3 and collected[1] == 42 and collected[2] == "via_pipe:42" and collected[3] == 99,
    "after cancel, tmp still gets its own messages but pipe listener is gone")

-- Cancelling a single-element pipe is a no-op (no connections to remove)
local c2 = pipe(tmp)
assert(type(c2) == "function", "pipe(single) returns cancel function")
c2()  -- should not error

-- bytes: immutable binary buffer
local b = bytes("\1\2\255")
assert(#b == 3 and b:size() == 3)
assert(b[1] == 1 and b[2] == 2 and b[3] == 255 and b[4] == nil)
assert(b[-1] == 255)
assert(b:byte(2) == 2)
assert(b:str() == "\1\2\255")
assert(b:hex() == "0102ff")
assert(b == bytes({ 1, 2, 255 }))
assert(b ~= bytes("\1\2"))
assert(b:sub(2) == bytes("\2\255"))
assert(b:sub(1, -2) == bytes("\1\2"))
assert(b .. bytes("\3") == bytes("\1\2\255\3"))
assert((b .. "\3"):size() == 4)
assert(tostring(b) == "bytes[3]: 01 02 ff")
assert(fmt("{}", b) == "bytes[3]: 01 02 ff")
assert(not pcall(function() b.x = 1 end), "bytes must be immutable")
assert(not pcall(bytes, { 300 }), "non-byte item must fail")
assert(bytes():size() == 0 and tostring(bytes()) == "bytes[0]")

local test_func = function() end

assert(type(pipe(test_func)) == "function", "pipe(func) returns cancel function")
assert(type(pipe(test_func, test_func)) == "function", "pipe(func, func) returns cancel function")

-- Extra interop
-- TestWorker:Call(func), calls func with 3 args
local called = {}
test:Call(function(a, b, c)
    log("{} - {} - {}", a, b, c)
    called = {a, b, c}
end)
assert(called[1] == 1 and called[2] == 2 and called[3] == 3,
    "Call must invoke the function with (1, 2, 3)")

log "Basic test OK"
shutdown()
