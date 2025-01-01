function call_all(table, ...)
    assert(type(table) == "table", "table expected as first arg")
    for _, v in ipairs(table) do
        v(...)
    end
end

function wrap(key)
    assert(type(key) == "string", "string expected as first arg")
    return function(msg)
        return set({}, key, msg)
    end
end

function unwrap(key)
    assert(type(key) == "string", "string expected as first arg")
    return function(msg)
        return get(msg, key)
    end
end

function filter(pattern)
    return function (msg)
        if type(msg) ~= "table" then
            return nil
        end
        local result = {}
        local hit = false
        for k, v in pairs(msg) do
            if string.match(k, pattern) == k then
                hit = true
                result[k] = v
            end
        end
        return hit and result or nil
    end
end

local function wrap_func(f)
    return {
        __wrap = f,
        __listeners = {},
        get_listeners = function(self)
            return self.__listeners
        end,
        call = function(self, msg)
            local temp = self.__wrap(msg)
            if temp ~= nil then
                call_all(self.__listeners, temp)
            end
        end
    }
end

local function connect(target, ipipe)
    local all = target:get_listeners()
    assert(type(all) == "table", ":get_listeners() should return a table")
    all[#all + 1] = function(msg)
        local ok, err = pcall(ipipe.call, ipipe, msg)
        if not ok then
            log.error("In (Pipe): {}", err)
        end
    end
end

function pipe(first, ...)
    assert(first ~= nil, "expected at least 1 argument")
    local toPipe
    if type(first) == 'table' then
        toPipe = first
    else
        toPipe = {first, ...}
    end
    local res = nil
    local curr = nil
    for i, v in ipairs(toPipe) do
        local t = type(v)
        if t == "function" then
            v = wrap_func(v)
        elseif t ~= "table" and t ~= "userdata" then
            error("function, table or userdata expected at arg "..i)
        end
        if curr ~= nil then
            connect(curr, v)
        end
        curr = v
        if res == nil then
            res = curr
        end
    end
    assert(res ~= nil, "expected at least on param")
    return res
end

-- Cross-version compatability
unpack = unpack or table.unpack
table.unpack = table.unpack or unpack