-- Pure-Lua pipeline dispatch: what a message costs per hop when no C++ boundary
-- is crossed. Contrast with bench_conversion.lua, where every hop converts the
-- whole message tree between QVariant and Lua.
--
-- Each timed body stores its result in the global BENCH_SINK (and rotates over a
-- pool of messages) so LuaJIT cannot eliminate the work as dead code.

local B = require "bench_lib"

local POOL = 8

local function source()
    return create_worker(function(self, msg, sender)
        notify_all(self, msg, sender)
    end)
end

local function pool(make)
    local t = {}
    for i = 1, POOL do
        t[i] = make()
    end
    return t
end

local function pick(t, i)
    return t[(i % POOL) + 1]
end

return function()
    print("\n[pipeline: pure Lua dispatch]")
    local section = "pipeline (Lua)"

    for _, depth in ipairs {1, 2, 4, 8} do
        local msgs = pool(function()
            return B.tree(depth, 1)
        end)
        for _, hops in ipairs {1, 3, 5} do
            local src = source()
            local fns = {}
            for i = 1, hops - 1 do
                fns[i] = function(m)
                    m.hops = (m.hops or 0) + 1
                    return m
                end
            end
            fns[hops] = function(m)
                m.hops = (m.hops or 0) + 1
                BENCH_SINK = m.hops
                return m
            end
            pipe(src, table.unpack(fns))
            B.bench(section, string.format("stamp chain depth=%d hops=%d", depth, hops),
                function(i)
                    src(pick(msgs, i))
                end, {meta = {depth = depth, hops = hops}})
        end
    end

    -- The common field-routing pattern: unwrap(payload) -> handler -> wrap(payload).
    for _, depth in ipairs {1, 4} do
        local inner = B.tree(depth, 1)
        local msgs = pool(function()
            return {payload = inner}
        end)
        local src = source()
        pipe(src, unwrap("payload"), function(m)
            BENCH_SINK = m
            return m
        end, wrap("payload"))
        B.bench(section, string.format("unwrap/wrap hop depth=%d", depth),
            function(i)
                src(pick(msgs, i))
            end, {meta = {depth = depth}})

        B.bench(section, string.format("wrap only depth=%d", depth),
            function(i)
                BENCH_SINK = wrap("payload")(pick(msgs, i))
            end, {meta = {depth = depth}})

        B.bench(section, string.format("unwrap only depth=%d", depth),
            function(i)
                BENCH_SINK = unwrap("payload")(pick(msgs, i))
            end, {meta = {depth = depth}})
    end

    -- filter() keeps one path and drops the rest; cost grows with path depth.
    for _, depth in ipairs {1, 2, 4, 8} do
        local msgs = pool(function()
            return B.tree(depth, 1)
        end)
        local path = string.rep("a:", depth - 1) .. "a"
        local f = filter(path)
        B.bench(section, string.format("filter() path depth=%d", depth),
            function(i)
                BENCH_SINK = f(pick(msgs, i))
            end, {meta = {depth = depth}})
    end
end
