-- Path indexing cost in Lua, and the lazy message view prototype:
--   * direct table indexing vs get()/filter()/wrap() path helpers, by depth
--   * eager QVariant -> Lua conversion of a whole message vs creating a lazy
--     view and reading one leaf through it (the pattern lazy indexing targets)
--
-- Timed bodies rotate over a small pool of inputs and write BENCH_SINK so the
-- LuaJIT build cannot hoist or eliminate the measured work.

local B = require "bench_lib"

local POOL = 8

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

local function read_eager(msg, depth)
    local node = msg
    for _ = 1, depth do
        node = node.a
    end
    return node[1]
end

local function read_lazy(node, depth)
    for _ = 1, depth do
        node = node.a
    end
    return node[1]
end

return function(env)
    print("\n[indexing: Lua path access]")
    local section = "indexing (Lua)"

    for _, depth in ipairs {1, 2, 4, 8} do
        local msgs = pool(function()
            return B.tree(depth, 1)
        end)
        local path = B.tree_path(depth)
        B.bench(section, string.format("direct index depth=%d", depth),
            function(i)
                BENCH_SINK = read_eager(pick(msgs, i), depth)
            end, {meta = {depth = depth}})

        B.bench(section, string.format("get() path depth=%d", depth),
            function(i)
                BENCH_SINK = get(pick(msgs, i), path)
            end, {meta = {depth = depth}})

        local f = filter(string.rep("a:", depth - 1) .. "a")
        B.bench(section, string.format("filter() path depth=%d", depth),
            function(i)
                BENCH_SINK = f(pick(msgs, i))
            end, {meta = {depth = depth}})
    end

    local depth = 4
    local inner = B.tree(depth, 1)
    local bare = pool(function()
        return inner
    end)
    local wrapped = pool(function()
        return {payload = inner}
    end)
    B.bench(section, "wrap('payload') depth=4",
        function(i)
            BENCH_SINK = wrap("payload")(pick(bare, i))
        end, {meta = {depth = depth}})
    B.bench(section, "unwrap('payload') depth=4",
        function(i)
            BENCH_SINK = unwrap("payload")(pick(wrapped, i))
        end, {meta = {depth = depth}})

    local bench = env.bench
    if not bench then
        print("\n[lazy view]  skipped: radapter_bench_plugin not loaded")
        return
    end

    print("\n[lazy view: eager conversion vs lazy leaf read]")
    local lazy_section = "lazy view"
    local cases = {
        {depth = 1, width = 1},
        {depth = 2, width = 1},
        {depth = 3, width = 1},
        {depth = 4, width = 1},
        {depth = 6, width = 1},
        {depth = 1, width = 8},
        {depth = 2, width = 8},
        {depth = 1, width = 64},
        {depth = 2, width = 32},
        {depth = 3, width = 16},
    }

    for _, case in ipairs(cases) do
        local d, width = case.depth, case.width
        local nodes = B.tree_nodes(d, width)
        local label = string.format("depth=%d width=%d nodes=%d", d, width, nodes)
        bench:Prepare(d, width)

        local eager = pool(function()
            return bench:DeepCached()
        end)
        local lazy = pool(function()
            return bench:LazyView()
        end)
        local lazy_cached = pool(function()
            return bench:LazyViewCached()
        end)
        B.bench(lazy_section, "eager convert, no read       " .. label,
            function()
                BENCH_SINK = bench:DeepCached()
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "lazy view, no read           " .. label,
            function()
                BENCH_SINK = bench:LazyView()
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "eager convert + read leaf    " .. label,
            function(i)
                BENCH_SINK = read_eager(bench:DeepCached(), d)
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "lazy view + read leaf        " .. label,
            function(i)
                BENCH_SINK = read_lazy(bench:LazyView(), d)
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "read leaf (prebuilt eager)   " .. label,
            function(i)
                BENCH_SINK = read_eager(pick(eager, i), d)
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "read leaf (prebuilt lazy)    " .. label,
            function(i)
                BENCH_SINK = read_lazy(pick(lazy, i), d)
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "read leaf (prebuilt cached)  " .. label,
            function(i)
                BENCH_SINK = read_lazy(pick(lazy_cached, i), d)
            end, {meta = {nodes = nodes}})

        B.bench(lazy_section, "materialize via proxy        " .. label,
            function(i)
                BENCH_SINK = pick(lazy, i):materialize()
            end, {meta = {nodes = nodes}})
    end
end
