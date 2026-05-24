---@diagnostic disable: lowercase-global
local co = coroutine

---@alias callback<T> fun(res: T?, err: string?)
---@alias promise<T> fun(cb: callback<T>)

---@return promise
local function make_promise(thread, ...)
    local cb
    local done
    local res, err
    local step
    step = function (...) -- Args or (res, err) here
        ok, res, err = co.resume(thread, ...)
        if not ok then
            err, res = res, nil
        end
        if co.status(thread) == "dead" then
            done = true
            if cb then cb(res, err) end
            return
        else
            res(step)
        end
    end
    step(...)
    return function (callback)
        if done then
            callback(res, err)
        else
            cb = callback
        end
    end
end


---@generic T
---@param func fun(...): T?
---@return fun(...): promise<T?>
function async(func)
    return function(...)
        local thread = co.create(func)
        local params = {...}
        local promise = make_promise(thread, ...)
        local last = params[#params]
        if type(last) == "function" then
            promise(last)
        else
            return promise
        end
    end
end

---@generic T
---@overload fun(...)
---@return fun(...): promise<T>
function promisify(func)
    return function(...)
        local cb
        local ok, res, err
        local params = {...}
        local last = params[#params]
        if type(last) ~= "function" then
            table.insert(params, function (_res, _err)
                if cb then
                    cb(_res, _err)
                else
                    res, err = _res, _err
                end
            end)
        end
        local thread = co.create(func)
        ok, err = co.resume(thread, unpack(params))
        if ok then err = nil end
        if type(last) ~= "function" then
            return function (callback)
                if res ~= nil or err ~= nil then
                    callback(res, err)
                else
                    cb = callback
                end
            end
        end
    end
end

---@generic T
---@param promise promise<T>
function await(promise)
    assert(type(promise) == "function", "function expected")
    return co.yield(promise)
end