-- C++ <-> Lua message conversion cost, measured through the benchmark plugin's
-- `Bench` worker:
--   * Sink     - incoming QVariantList: full Lua table -> QVariant tree (one way)
--   * Deep     - returns a QVariant tree: full QVariant -> Lua table (other way)
--   * Roundtrip- returns its first argument: both directions in one call
--   * Echo     - emits the message back to its Lua listeners: QVariant -> Lua
--                plus the real worker notification path (fiber entry, call_all)

local B = require "bench_lib"

local CASES = {
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

return function(env)
    local bench = env.bench
    if not bench then
        print("\n[conversion: C++ <-> Lua]  skipped: radapter_bench_plugin not loaded")
        return
    end
    print("\n[conversion: C++ <-> Lua message trees]")
    local section = "conversion (C++<->Lua)"

    -- Fixed cost of an extra-method call with no conversion at all.
    B.bench(section, "extra-method call floor (Noop)",
        function()
            bench:Noop()
        end)

    for _, case in ipairs(CASES) do
        local depth, width = case.depth, case.width
        bench:Prepare(depth, width)
        local msg = bench:DeepCached()
        local nodes = B.tree_nodes(depth, width)
        local label = string.format("depth=%d width=%d nodes=%d", depth, width, nodes)

        B.bench(section, "Lua table -> QVariant   " .. label,
            function()
                bench:Sink(msg)
            end, {meta = {nodes = nodes, depth = depth, width = width}})

        B.bench(section, "QVariant -> Lua table   " .. label,
            function()
                BENCH_SINK = bench:DeepCached()
            end, {meta = {nodes = nodes, depth = depth, width = width}})

        B.bench(section, "roundtrip (both ways)   " .. label,
            function()
                BENCH_SINK = bench:Roundtrip(msg)
            end, {meta = {nodes = nodes, depth = depth, width = width}})
    end

    -- Echo: the same QVariant->Lua conversion, but through the worker
    -- notification path (a new fiber entry + call_all over the listener list).
    local depth, width = 2, 4
    local msg = bench:Deep(depth, width)
    local received = 0
    local cancel_echo = pipe(bench, function()
        received = received + 1
    end)
    B.bench(section, "worker echo (notify + convert) depth=2 width=4",
        function()
            bench:Echo(msg)
        end, {meta = {depth = depth, width = width, received = received}})
    cancel_echo()

    -- Forwarding: a message reaching a Lua handler and being handed to the next
    -- C++ worker costs one full conversion pair per hop. Chain length 1 vs 2
    -- gives the marginal cost.
    local sink = Bench {name = "bench_sink"}
    local sink2 = Bench {name = "bench_sink2"}
    local cancel_one = pipe(bench, function(m)
        sink(m)
    end)
    B.bench(section, "forward 1 hop (convert both ways)",
        function()
            bench:Echo(msg)
        end, {meta = {depth = depth, width = width}})
    cancel_one()

    local cancel_two = pipe(bench, function(m)
        sink2(m)
        sink(m)
    end)
    B.bench(section, "forward 2 hops (convert both ways twice)",
        function()
            bench:Echo(msg)
        end, {meta = {depth = depth, width = width}})
    cancel_two()

    -- Full pipeline across C++ worker boundaries: every hop is
    -- QVariant -> Lua (notification) + Lua table -> QVariant (next worker call).
    print("\n[pipeline across C++ workers]")
    local pipe_section = "pipeline (C++ hops)"
    local chain_id = 0
    local function build_chain(hops)
        chain_id = chain_id + 1
        local workers = {}
        for i = 1, hops do
            workers[i] = Bench {name = "bench_pipe_" .. chain_id .. "_" .. i}
        end
        for i = 1, hops - 1 do
            pipe(workers[i], function(m)
                workers[i + 1]:Echo(m)
            end)
        end
        local count = {n = 0}
        pipe(workers[hops], function(m)
            count.n = count.n + 1
            BENCH_SINK = m
        end)
        return workers[1], count
    end

    for _, depth in ipairs {1, 2, 4, 8} do
        local msg = bench:Deep(depth, 1)
        for _, hops in ipairs {1, 2, 3} do
            local head = build_chain(hops)
            B.bench(pipe_section, string.format("depth=%d hops=%d", depth, hops),
                function()
                    head:Echo(msg)
                end, {meta = {depth = depth, hops = hops}})
        end
    end
end
