---@diagnostic disable: lowercase-global
local co = coroutine

---@alias callback<T> fun(res: T?, err: string?)
---@alias promise<T> fun(cb: callback<T>)

local function is_callable(v)
    if type(v) == "function" then return true end
    local mt = getmetatable(v)
    return mt ~= nil and mt.__call ~= nil
end

local promise_mt = {
    __call = function(self, callback)
        self.cb = callback
        if self.done then
            callback(self.res, self.err)
        end
    end
}

-- Subscription is synchronous in practice (await resumes and subscribes
-- immediately; explicit promise(cb) follows the call on the same line), so a
-- rejected promise still unsubscribed one event-loop turn later is
-- fire-and-forget: report it instead of swallowing the error.
local function unhandled_check(p)
    after(0, function()
        if not p.cb then
            error("Unhandled async error: " .. tostring(p.err), 0)
        end
    end)
end

---@return promise
local function make_promise(thread, ...)
    local p = setmetatable({}, promise_mt)
    local step
    local function settle(res, err)
        p.done, p.res, p.err = true, res, err
        if p.cb then
            p.cb(res, err)
        elseif err ~= nil then
            unhandled_check(p)
        end
    end
    step = function (...) -- Args or (res, err) here
        local ok, res, err = co.resume(thread, ...)
        if not ok then
            settle(nil, debug.traceback(thread, res))
            return
        end
        if co.status(thread) == "dead" then
            settle(res, err)
        else
            res(step)
        end
    end
    step(...)
    return p
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
        local res, err
        local params = {...}
        local last = params[#params]
        local inline = type(last) == "function"
        if not inline then
            table.insert(params, function (_res, _err)
                if cb then
                    cb(_res, _err)
                else
                    res, err = _res, _err
                end
            end)
        end
        local thread = co.create(func)
        local ok, rerr = co.resume(thread, unpack(params))
        if not ok then
            rerr = debug.traceback(thread, rerr)
            if inline then
                last(nil, rerr)
                return
            end
            err = rerr
        end
        if not inline then
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
    assert(is_callable(promise), "callable (promise) expected")
    return co.yield(promise)
end

-- Runs a main chunk (Eval/EvalFile) inside a coroutine so that await() works
-- at the top level. Errors before the first await are re-raised synchronously
-- (startup failures must still fail Eval); later errors are logged.
function __eval_async(chunk, chunkname)
    local sync_phase = true
    local sync_err = nil
    local p = async(chunk)()
    p(function(_, err)
        if sync_phase then
            sync_err = err
        elseif err ~= nil then
            log.error("In ({}): {}", chunkname, err)
        end
    end)
    sync_phase = false
    if sync_err ~= nil then
        error(sync_err, 0)
    end
end
