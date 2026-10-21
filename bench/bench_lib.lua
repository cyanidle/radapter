-- Shared benchmark helpers: CPU-time measurement with calibration and repeats,
-- canonical nested message trees, and a results table.
--
-- Everything measures CPU time (os.clock), so the numbers are independent of
-- machine load and of the event loop being idle.

local M = {}

os.setlocale("C")

local clock = os.clock

function M.now()
    return clock()
end

local function median(values)
    table.sort(values)
    local n = #values
    if n % 2 == 1 then
        return values[math.floor((n + 1) / 2)]
    end
    return (values[n / 2] + values[n / 2 + 1]) / 2
end

-- Returns seconds per call of fn(i). Calibrates the iteration count so one
-- sample takes at least `min_s` seconds, then takes `samples` samples and
-- reports the median. The collector runs during measurement (realistic), it
-- is only forced before each sample so samples do not inherit garbage.
function M.rate(fn, opts)
    opts = opts or {}
    local min_s = opts.min_s or 0.05
    local samples = opts.samples or 5
    local max_n = opts.max_n or 50000000
    local n = opts.n or 128
    for i = 1, math.min(n, 64) do
        fn(i)
    end
    local dt
    while true do
        collectgarbage("collect")
        local t0 = clock()
        for i = 1, n do
            fn(i)
        end
        dt = clock() - t0
        if dt >= min_s or n >= max_n then
            break
        end
        n = n * 4
        if n > max_n then n = max_n end
    end
    local times = {}
    for s = 1, samples do
        collectgarbage("collect")
        local t0 = clock()
        for i = 1, n do
            fn(i)
        end
        times[s] = clock() - t0
    end
    return median(times) / n, n
end

-- Nested message matching the Bench plugin's buildDeep(): `depth` levels of
-- `a = {...}` ending in a list of `width` integers, plus keys k1..k(width-1)
-- as siblings on every level.
function M.tree(depth, width)
    width = width or 1
    local node
    if depth <= 0 then
        node = {}
        for i = 1, width do
            node[i] = i
        end
        return node
    end
    node = { a = M.tree(depth - 1, width) }
    for i = 2, width do
        node["k" .. (i - 1)] = i
    end
    return node
end

-- Number of table nodes in M.tree(depth, width).
function M.tree_nodes(depth, width)
    width = width or 1
    return depth * width + width
end

-- "a:a:...:a:[1]" for M.tree(depth)
function M.tree_path(depth)
    if depth <= 0 then
        return "[1]"
    end
    return string.rep("a:", depth) .. "[1]"
end

-- "a.a.a...a[1]", the direct-index spelling of M.tree_path()
function M.tree_index_expr(depth)
    return string.rep(".a", depth) .. "[1]"
end

function M.tree_child(msg, depth)
    local node = msg
    for _ = 1, depth do
        node = node.a
    end
    return node
end

function M.records()
    return M._records
end

function M.reset()
    M._records = {}
end

local function fmt_ns(ns)
    if ns >= 1e6 then
        return string.format("%.2f ms", ns / 1e6)
    elseif ns >= 1e3 then
        return string.format("%.2f us", ns / 1e3)
    end
    return string.format("%.1f ns", ns)
end

function M.record(section, name, sec, opts)
    opts = opts or {}
    local ns = sec * 1e9
    M._records = M._records or {}
    M._records[#M._records + 1] = {
        section = section, name = name, ns = ns,
        meta = opts.meta or {}, n = opts.n,
    }
    if not opts.quiet then
        local line = string.format("  %-52s %12s", name, fmt_ns(ns))
        if opts.meta and opts.meta.note then
            line = line .. "   " .. tostring(opts.meta.note)
        end
        io.stderr:write(line, "\n")
        io.stderr:flush()
    end
end

function M.bench(section, name, fn, opts)
    opts = opts or {}
    local sec, n = M.rate(fn, opts)
    M.record(section, name, sec, {meta = opts.meta, n = n, quiet = opts.quiet})
    return sec
end

function M.print_table()
    local section = nil
    print("")
    print(string.rep("=", 96))
    for _, r in ipairs(M._records or {}) do
        if r.section ~= section then
            section = r.section
            print("")
            print("[" .. section .. "]")
        end
        print(string.format("  %-52s %12s", r.name, fmt_ns(r.ns)))
    end
    print("")
    print(string.rep("=", 96))
end

return M
