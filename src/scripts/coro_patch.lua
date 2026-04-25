local orig_resume = coroutine.resume
local orig_traceback = debug.traceback

-- magic here! Take care of main thread.
local mainthr = coroutine.running()      -- captureing is a must.
debug.traceback = function(athr, msg, lvl)
  if athr then return orig_traceback(athr, msg, lvl) end  -- no interest in specified thread.
  return orig_traceback(mainthr, msg, lvl)
end

coroutine.resume = function(thr, ...)
  -- another magic.
  local uptrace = debug.traceback
  debug.traceback = function(athr, msg, lvl)
    if athr and type(athr) == "thread" then return orig_traceback(athr, msg, lvl) end  -- no interest in specified thread.
    return orig_traceback(thr, msg, lvl)     -- trace the stack of thr.
      .. '\n' .. uptrace(msg, lvl) -- trace from thr's resume point.
  end

  local result = { orig_resume(thr, ...) }
  debug.traceback = uptrace
  return table.unpack(result)
end