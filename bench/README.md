# radapter performance benchmarks

Self-contained benchmarks for the costs that dominate a typical radapter
pipeline:

1. **message trees of different depths and widths** — how much the shape of a
   message costs as it moves through Lua and across the C++ boundary;
2. **C++ <-> Lua conversion** — what a full `QVariant` <-> Lua table conversion
   costs, and what the engine's lazy message view saves instead;
3. **path indexing** — `get()` / `filter()` / `wrap()` / `unwrap()` versus
   direct table indexing, and the lazy view: field reads, `pairs`, `#`,
   `tostring` (JSON), forwarding and `materialize()`.

Numbers from a reference run are in [`RESULTS.md`](RESULTS.md); the design of
the lazy view is in [`LAZY_INDEXING.md`](LAZY_INDEXING.md).

## Running

```bash
# PUC Lua 5.4 build (reference)
build/bin/radapter bench/run_all.lua

# LuaJIT build
build-jit/bin/radapter bench/run_all.lua
```

The suite takes ~2 minutes of CPU time and needs little memory (< 100 MB RSS).
Progress lines go to stderr, the final consolidated table to stdout:

```bash
build/bin/radapter bench/run_all.lua > bench/results-build.txt
```

`radapter_bench_plugin` is built next to the executable by the normal build
(`libradapter_bench_plugin.so`); `run_all.lua` loads it by short name. Without
it the Lua-only sections still run and the conversion/lazy sections report that
they were skipped.

## What is measured

| file | section | measures |
|---|---|---|
| `bench_pipeline.lua` | `pipeline (Lua)` | dispatch through `pipe()` and plain Lua handlers: 1/3/5 hops, depth 1..8; `wrap`/`unwrap`/`filter` |
| `bench_conversion.lua` | `conversion (C++<->Lua)` | full-message conversion each way and roundtrip, worker echo, forwarding a received view versus a materialized table |
| | `pipeline (C++ hops)` | end-to-end message flow across 1..3 worker boundaries |
| `bench_indexing.lua` | `indexing (Lua)` | direct index vs `get()` vs `filter()` vs `wrap`/`unwrap` |
| | `lazy view` | real worker notifications of the same message: eager conversion vs lazy view, reading one leaf, `pairs`, `tostring`, forwarding and `materialize` |

Messages are built by `bench_lib.tree(depth, width)` / the plugin's
`buildDeep()`: `depth` nested maps keyed `a`, each level carrying `width - 1`
integer siblings, and a leaf list of `width` integers. `nodes` in the output is
`depth * width + width` — the number of tables in the message.

## Methodology

- **CPU time, not wall time** (`os.clock`), so results do not depend on machine
  load or event-loop scheduling.
- Each measurement calibrates the iteration count to at least 50 ms, then takes
  5 samples and reports the **median**; the collector is forced before each
  sample but runs during it (allocation costs are part of the result).
- Timed bodies rotate over a pool of 8 messages and store their result in the
  global `BENCH_SINK`, so LuaJIT cannot hoist or discard the work.
- The `lazy view` section captures the view from a **real** notification
  (`pipe(bench, function(m) view = m end); bench:EchoCached()`), so it measures
  the engine's own message path, not a benchmark-only shortcut.
- Microbenchmarks are single-threaded, single-process, no I/O.

### LuaJIT caveat

Conversion numbers are stable across builds. Pure-Lua dispatch microbenchmarks
are not: LuaJIT may inline one pipeline trace and fall back to the interpreter
for the next bytecode-identical one, so bytecode-identical rows differ by up to
~20%. Treat the LuaJIT column as "what the JIT can do" and the PUC column as
the stable reference.

## Adding a benchmark

Use `bench_lib`:

```lua
local B = require "bench_lib"

B.bench("my section", "case name", function(i)
    -- work for one message; use i to rotate inputs
    BENCH_SINK = my_operation(my_inputs[(i % 8) + 1])
end, {meta = {nodes = 42}})
```

`M.rate(fn)` returns `(seconds_per_call, iterations)` if you need the raw
number. `M.print_table()` prints everything recorded.

## Benchmark plugin API

`bench/bench_plugin.cpp` registers a `Bench` worker (load with
`load_plugin("radapter_bench_plugin")`). Everything it emits reaches Lua
listeners through the engine's normal notification path, i.e. as a lazy view:

| method | meaning |
|---|---|
| `bench:Sink(msg)` | converts the Lua message to `QVariant` and drops it (Lua -> QVariant) |
| `bench:Roundtrip(msg)` | converts to `QVariant` and back (both directions) |
| `bench:Echo(msg)` | emits the message to its Lua listeners (notification + view) |
| `bench:Deep(depth, width)` | builds and returns a fresh message tree (build + QVariant -> Lua) |
| `bench:Prepare(depth, width)` | retains a message tree in C++ for the `*Cached` methods |
| `bench:DeepCached()` | returns the retained tree (full QVariant -> Lua conversion) |
| `bench:EchoCached()` | notifies listeners with the retained tree (notification + lazy view) |
| `bench:Noop()` | call-floor measurement |

A message that arrives through a listener is a `LazyNode` userdata over the
retained `QVariant`; see [`LAZY_INDEXING.md`](LAZY_INDEXING.md) for the exact
semantics (`__index`, `__len`, `pairs`/`ipairs`, `tostring` as JSON,
copy-on-write `__newindex`, `materialize()`).

## Files

- `bench_lib.lua` — timing, median, message builders, results table
- `bench_pipeline.lua`, `bench_conversion.lua`, `bench_indexing.lua` — sections
- `run_all.lua` — entry point
- `bench_plugin.cpp` — C++ boundary worker for the benchmarks
- `RESULTS.md` — reference numbers and derived costs
- `LAZY_INDEXING.md` — the lazy message view: design, semantics, measured gains
