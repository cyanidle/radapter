local deep = {
    a = { b = { c = 1 }  }
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

assert(not pcall(pipe), "Empty pipe() should fail")
assert(not pcall(pipe, {}), "Empty pipe{} should fail")

assert(pipe {test} == test, "pipe{x} == x")
assert(pipe (test) == test, "pipe(x) == x")

assert(pcall(pipe(function () end)), "convert function to callable")

local first = pipe {
    test,
    function () end,
}

assert(first == test, "pipe{x, y} should return x")

pipe {
    test,
    function () return 1 end,
    function (msg) return msg end,
}

first = pipe (
    test,
    function(msg)
        log(msg)
        return msg + 1
    end,
    function(msg)
        log(msg)
        --return msg + 1
    end,
    function(msg)
        log(msg)
        error("Should not be reachable")
    end
)

assert(first == test, "pipe(x, y, z) should return x")

local test_func = function () end

assert(pipe { test_func } ~= test_func, "pipe{func} -> wrapper for func)")
assert(pipe { test_func, test_func } ~= test_func, "pipe{func} -> wrapper for func)")
assert(pipe(test_func) ~= test_func, "pipe(func) -> wrapper for func")
assert(pipe(test_func, test_func) ~= test_func, "pipe(func) -> wrapper for func")

test
:pipe(function()
    return deep
end)
:pipe(function(msg)
    return msg
end)

-- >> syntax for pipes in lua 5.3+

-- _ = test 
-- >> function (msg)
--     return msg + 10
-- end 
-- >> test

test:Call(function(a, b, c)
    log("{} - {} - {}", a, b, c)
end)
