-- ! https://github.com/ms-jpq/lua-async-await
-- ! Forked by me (github user): cyanidle

--#################### ############ ####################
--#################### Async Region ####################
--#################### ############ ####################

local co = coroutine

-- ADor
-- Updated by me to allow multiple return values

-- use with wrap
local pong = function (func, callback)
  assert(type(func) == "function", "type error :: expected func")
  local thread = co.create(func)
  local step = nil
  step = function (...)
    local pack = {co.resume(thread, ...)}
    local status = pack[1]
    local ret = pack[2]
    if not status then
      error(debug.traceback(thread, ret, 2))
    end
    if co.status(thread) == "dead" then
        if (callback) then 
          (function (_, ...) callback(...) end)(table.unpack(pack))
        end
    else
      assert(type(ret) == "function", "type error :: expected func - coroutine yielded some value")
      ret(step)
    end
  end
  step()
end


-- use with pong, creates thunk factory
local wrap = function (func)
  assert(type(func) == "function", "type error :: expected func")
  local factory = function (...)
    local params = {...}
    local thunk = function (step)
      table.insert(params, step)
      return func(unpack(params))
    end
    return thunk
  end
  return factory
end


-- many thunks -> single thunk
local join = function (thunks)
  local len = table.getn(thunks)
  local done = 0
  local acc = {}

  local thunk = function (step)
    if len == 0 then
      return step()
    end
    for i, tk in ipairs(thunks) do
      assert(type(tk) == "function", "thunk must be function")
      local callback = function (...)
        acc[i] = {...}
        done = done + 1
        if done == len then
          step(unpack(acc))
        end
      end
      tk(callback)
    end
  end
  return thunk
end


-- sugar over coroutine
local await = function (defer)
  assert(type(defer) == "function", "type error :: expected func")
  return co.yield(defer)
end


local await_all = function (defer)
  assert(type(defer) == "table", "type error :: expected table")
  return co.yield(join(defer))
end

local async_after = wrap(after)

return {
  sync = wrap(pong),
  wait = await,
  wait_all = await_all,
  wrap = wrap,
  sleep = function (time) 
    await(async_after(time))
  end,
}
