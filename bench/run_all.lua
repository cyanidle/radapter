-- Runs the whole benchmark suite. Usage:
--   build/bin/radapter bench/run_all.lua
--
-- The C++ boundary benchmarks need radapter_bench_plugin, built next to the
-- executable as libradapter_bench_plugin.so; without it those sections are
-- skipped and the Lua-only sections still run.

local B = require "bench_lib"

local function load_bench_plugin()
    local ok, err = pcall(load_plugin, "radapter_bench_plugin")
    if not ok then
        print("WARNING: could not load radapter_bench_plugin: " .. tostring(err))
        return nil
    end
    return Bench {name = "bench"}
end

local env = {
    bench = load_bench_plugin(),
}

local t0 = B.now()
require "bench_pipeline" (env)
if env.bench then
    require "bench_conversion" (env)
end
require "bench_indexing" (env)
local elapsed = B.now() - t0

if env.bench then
    env.bench:destroy()
end

B.print_table()
print(string.format("total CPU time: %.2f s", elapsed))
shutdown()
