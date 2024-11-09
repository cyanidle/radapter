local xresume = coroutine.resume
local xtrace = debug.traceback

-- magic here! Take care of main thread.
local mainthr = coroutine.running()      -- captureing is a must.
debug.traceback = function(athr)
  if athr then return xtrace(athr) end  -- no interest in specified thread.
  return xtrace(mainthr)
end

coroutine.resume = function(thr, ...)
  -- another magic.
  local uptrace = debug.traceback
  debug.traceback = function(athr)
    if athr then return xtrace(athr) end  -- no interest in specified thread.
    return xtrace(thr)     -- trace the stack of thr.
      .. '\n' .. uptrace() -- trace from thr's resume point.
  end
  
  local result = { xresume(thr, ...) }
  debug.traceback = uptrace
  return table.unpack(result)
end