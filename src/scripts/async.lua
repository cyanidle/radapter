---@diagnostic disable: lowercase-global
-- Promise plumbing over the native fiber pool. Every Lua entry point already
-- runs on a fiber, so any function may call promise:await() directly - there
-- are no Lua coroutines and no async()/await() wrappers:
--   * promise(executor) - wrap callback-style async: executor(done) runs
--     immediately; the first done(res, err) settles the promise
--   * spawn(fn, ...)   - run fn detached on its own fiber entry; the promise
--     settles with the result of the call, or (nil, traceback) if fn raises
--   * promisify(func)  - adapt a trailing-callback function to promises
--   * gather{...}      - await a list of promises concurrently; resolves with
--     a list of all results in the same order, rejects with the first error
--   * match_msg(src, f) - promise for the first pipe message matching f
-- A rejected promise with no subscriber is reported as an error one event
-- loop turn later, so fire-and-forget can not swallow failures silently.

---@alias callback<T> fun(res: T?, err: string?)

local function is_callable(v)
    if type(v) == "function" then return true end
    local mt = getmetatable(v)
    return mt ~= nil and mt.__call ~= nil
end

local promise_mt = {
    __call = function(self, callback)
        assert(is_callable(callback), "callback function expected")
        self._subscribe(callback)
    end,
    __index = {
        await = function(self)
            return __await_native(self._subscribe)
        end,
    },
}
debug.getregistry().radapter_promise_mt = promise_mt

local function make_state()
    local subs = {}
    local had_sub = false
    local settled = false
    local res, err

    local function subscribe(cb)
        had_sub = true
        if settled then
            cb(res, err)
        else
            subs[#subs + 1] = cb
        end
    end

    local function done(r, e)
        if settled then return end
        settled = true
        res, err = r, e
        if #subs > 0 then
            for i = 1, #subs do subs[i](res, err) end
        elseif e ~= nil then
            after(0, function()
                if not had_sub then
                    error("Unhandled async error: " .. tostring(e), 0)
                end
            end)
        end
    end

    return subscribe, done
end

---Create a promise around callback-style async: `executor(done)` runs
---immediately (it may subscribe listeners, start timers, issue requests);
---the first `done(res, err)` settles the promise.
---@generic T
---@param executor fun(done: callback<T>)
---@return promise<T>
function promise(executor)
    assert(is_callable(executor), "promise(executor): executor must be callable")
    local subscribe, done = make_state()
    local p = setmetatable({ _subscribe = subscribe }, promise_mt)
    local ok, rerr = xpcall(executor, debug.traceback, done)
    if not ok then
        done(nil, rerr)
    end
    return p
end

---Run `fn` detached from the current fiber, so it executes concurrently with
---the calling flow; returns a promise settling with the result of the call, or
---(nil, traceback) when fn raises. A plain call already suspends only the
---calling chain - spawn only to gain parallelism. The native spawn returns that
---promise itself; it is re-wrapped here so that a discarded failing spawn is
---still reported (the native promise does not check for unhandled rejections).
---@generic T
---@param fn fun(...): T?
---@return promise<T?>
function spawn(fn, ...)
    local args = table.pack(...)
    return promise(function(done)
        __spawn_native(fn, table.unpack(args, 1, args.n))(done)
    end)
end

---Await a list of promises concurrently: resolves with a list of all results
---in the same order (`{res1, res2, ...}`), or rejects with the first error.
---@generic T
---@param promises promise<T>[]
---@return promise<T[]>
function gather(promises)
    return promise(function(done)
        local n = #promises
        if n == 0 then return done({}) end
        local results, left = {}, n
        for i, p in ipairs(promises) do
            p(function(res, err)
                if err ~= nil then
                    done(nil, err)
                else
                    results[i] = res
                    left = left - 1
                    if left == 0 then done(results) end
                end
            end)
        end
    end)
end

---Adapt a trailing-callback function: the returned wrapper produces a promise
---settling with whatever the callback receives as (res, err).
---@generic T
---@param func fun(..., callback<T>)
---@return fun(...): promise<T>
function promisify(func)
    return function(...)
        local args = table.pack(...)
        return promise(function(done)
            args.n = args.n + 1
            args[args.n] = done
            func(table.unpack(args, 1, args.n))
        end)
    end
end

---Wait for a message matching a filter from a pipe-able source. The promise
---resolves with the first message for which `filter(msg)` returns true, then
---automatically unsubscribes.
---@generic T
---@param source table|userdata pipe-able source (has get_listeners)
---@param filter fun(msg: T): boolean
---@return promise<T>
function match_msg(source, filter)
    return promise(function(done)
        -- forward declaration: the listener must close over `cancel` as an
        -- upvalue, but a `local x = f(g)` only scopes x after the statement
        local cancel
        cancel = pipe(source, function(msg)
            if filter(msg) then
                cancel()
                done(msg)
            end
        end)
    end)
end
