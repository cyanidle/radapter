# Reference benchmark results

- **Date:** 2026-10-21
- **Machine:** AMD Ryzen 9 7900X3D (12C/24T), Linux 7.0.0-38-generic
- **Builds:** `build/` = PUC Lua 5.4.7 (reference), `build-jit/` = LuaJIT 2.1.0-beta3 (`RADAPTER_JIT=ON`)
- **Commands:**

```bash
cmake -B build -G Ninja && cmake --build build -j
cmake -B build-jit -G Ninja -D RADAPTER_JIT=ON && cmake --build build-jit -j
build/bin/radapter     bench/run_all.lua > bench/results-build.txt
build-jit/bin/radapter bench/run_all.lua > bench/results-jit.txt
```

Raw output: [`results-build.txt`](results-build.txt), [`results-jit.txt`](results-jit.txt).
`nodes` below is the number of tables in the message (`depth * width + width`).
`ns`/`µs` are CPU time per message, median of 5 samples.

All tables come from one run. Re-measuring the same code on this machine reproduces the
µs-range rows within ±5%; sub-microsecond rows (pure-Lua dispatch, `forward view`) move by up
to ~15% with CPU frequency, so treat small deltas there as noise.

## 1. Pipeline dispatch (pure Lua)

One `pipe()` hop is a Lua function call through `call_all`/`xpcall`; message
depth does not matter because Lua only passes the table reference. A hop that
stamps a field costs ~60% more than the pure dispatch floor.

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| stamp chain depth=1 hops=1 | 1.24 µs | 148.2 ns |
| stamp chain depth=1 hops=3 | 2.54 µs | 332.1 ns |
| stamp chain depth=1 hops=5 | 3.95 µs | 598.1 ns |
| stamp chain depth=2 hops=1 | 1.20 µs | 178.5 ns |
| stamp chain depth=2 hops=3 | 2.61 µs | 382.6 ns |
| stamp chain depth=2 hops=5 | 3.96 µs | 606.6 ns |
| stamp chain depth=4 hops=1 | 1.20 µs | 176.5 ns |
| stamp chain depth=4 hops=3 | 2.61 µs | 386.5 ns |
| stamp chain depth=4 hops=5 | 4.11 µs | 603.8 ns |
| stamp chain depth=8 hops=1 | 1.22 µs | 177.8 ns |
| stamp chain depth=8 hops=3 | 2.61 µs | 379.0 ns |
| stamp chain depth=8 hops=5 | 4.02 µs | 598.5 ns |
| unwrap/wrap hop depth=1 | 3.32 µs | 701.2 ns |
| wrap only depth=1 | 783.5 ns | 267.9 ns |
| unwrap only depth=1 | 617.8 ns | 200.7 ns |
| unwrap/wrap hop depth=4 | 3.27 µs | 712.9 ns |
| wrap only depth=4 | 774.7 ns | 267.3 ns |
| unwrap only depth=4 | 608.4 ns | 199.5 ns |
| filter() path depth=1 | 739.4 ns | 299.8 ns |
| filter() path depth=2 | 1.11 µs | 428.6 ns |
| filter() path depth=4 | 1.83 µs | 750.2 ns |
| filter() path depth=8 | 3.15 µs | 1.35 µs |

PUC: marginal dispatch is **~0.70 µs per hop**, independent of depth
(`hops=5 − hops=1` = 2.71 µs / 4). `wrap`/`unwrap` are O(1) (`set`/`get` on one
key), `filter()` is O(path depth) because it rebuilds the path.

> LuaJIT: rows still deviate from each other by ~20% depending on which trace
> got compiled, and the first `hops=1` row is the fastest of its family. Do not
> read small differences between LuaJIT rows; the PUC column is the stable
> reference.

## 2. C++ <-> Lua message conversion

`Lua -> QVariant` is `toQVar()` (recursive Lua table walk), `QVariant -> Lua`
is `glua::Push(QVariant)` (recursive table build). Both are eager and touch
every node of the message. They still run whenever a *script* builds a message
or asks for a plain table; a worker message no longer pays for them (see §5).

| case | Lua -> QVariant | QVariant -> Lua | roundtrip |
|---|---:|---:|---:|
| depth=1 width=1 nodes=2 | 1.82 µs | 1.37 µs | 2.70 µs |
| depth=2 width=1 nodes=3 | 2.54 µs | 1.99 µs | 4.00 µs |
| depth=3 width=1 nodes=4 | 3.22 µs | 2.62 µs | 5.28 µs |
| depth=4 width=1 nodes=5 | 3.90 µs | 3.08 µs | 6.58 µs |
| depth=6 width=1 nodes=7 | 5.09 µs | 4.30 µs | 9.09 µs |
| depth=1 width=8 nodes=16 | 8.53 µs | 3.72 µs | 11.73 µs |
| depth=2 width=8 nodes=24 | 14.63 µs | 6.22 µs | 20.69 µs |
| depth=1 width=64 nodes=128 | 67.07 µs | 21.92 µs | 93.31 µs |
| depth=2 width=32 nodes=96 | 64.31 µs | 21.04 µs | 84.40 µs |
| depth=3 width=16 nodes=64 | 44.61 µs | 15.86 µs | 60.01 µs |

LuaJIT, same cases:

| case | Lua -> QVariant | QVariant -> Lua | roundtrip |
|---|---:|---:|---:|
| nodes=2 | 1.49 µs | 980.4 ns | 2.10 µs |
| nodes=3 | 2.10 µs | 1.48 µs | 3.22 µs |
| nodes=4 | 2.66 µs | 2.01 µs | 4.39 µs |
| nodes=5 | 3.31 µs | 2.46 µs | 5.40 µs |
| nodes=7 | 4.27 µs | 3.40 µs | 7.39 µs |
| nodes=16 | 7.37 µs | 3.24 µs | 10.24 µs |
| nodes=24 | 12.75 µs | 5.67 µs | 18.35 µs |
| nodes=128 | 63.63 µs | 19.76 µs | 86.94 µs |
| nodes=96 | 58.55 µs | 19.86 µs | 78.20 µs |
| nodes=64 | 40.08 µs | 15.08 µs | 55.31 µs |

Least-squares fit over all cases:

| direction | PUC fixed | PUC per node | JIT fixed | JIT per node |
|---|---:|---:|---:|---:|
| Lua -> QVariant | 1.42 µs | 0.58 µs | 0.76 µs | 0.54 µs |
| QVariant -> Lua | 2.07 µs | 0.18 µs | 1.61 µs | 0.17 µs |
| roundtrip | 2.71 µs | 0.78 µs | 1.78 µs | 0.73 µs |

So a *single* deep message (depth 8, 9 nodes) costs **~10 µs** to cross one
boundary and back; a 100-node message costs **~80 µs**. Conversion dominates
everything else in a pipeline (a pure-Lua hop is 0.7 µs).

The extra-method call floor on the same worker is **0.49 µs** (PUC) / **0.36 µs**
(JIT) — conversion of even a two-node message is 3x that.

## 3. Pipeline across C++ worker boundaries

Each hop is a notification (message -> Lua listener, now a lazy view) plus a
call of the next worker (a view converts back to the retained tree for free).
Since neither direction walks the tree any more, a hop costs the same at every
depth:

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| depth=1 hops=1 | 5.61 µs | 3.63 µs |
| depth=1 hops=2 | 10.82 µs | 6.48 µs |
| depth=1 hops=3 | 15.32 µs | 9.27 µs |
| depth=2 hops=1 | 6.29 µs | 4.29 µs |
| depth=2 hops=2 | 11.37 µs | 7.10 µs |
| depth=2 hops=3 | 16.26 µs | 9.91 µs |
| depth=4 hops=1 | 7.92 µs | 5.48 µs |
| depth=4 hops=2 | 12.72 µs | 8.29 µs |
| depth=4 hops=3 | 17.90 µs | 11.25 µs |
| depth=8 hops=1 | 10.75 µs | 8.06 µs |
| depth=8 hops=2 | 15.57 µs | 10.73 µs |
| depth=8 hops=3 | 20.83 µs | 13.47 µs |

Marginal cost of one more hop is **~5.2 µs flat** (PUC) / **~2.7 µs** (JIT),
whereas before the lazy view it grew with the message: 6.4 µs at depth 1 and
15.7 µs at depth 8. Three hops at depth 8 cost 20.8 µs instead of 46 µs.

What remains is fixed per hop: fiber dispatch, `call_all`, the Lua handler and
the next worker call — not the message.

## 4. Path indexing in Lua

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| direct index depth=1 | 140.0 ns | 15.6 ns |
| get() path depth=1 | 395.1 ns | 164.1 ns |
| filter() path depth=1 | 789.5 ns | 319.5 ns |
| direct index depth=2 | 153.1 ns | 13.7 ns |
| get() path depth=2 | 446.3 ns | 202.7 ns |
| filter() path depth=2 | 1.13 µs | 470.4 ns |
| direct index depth=4 | 170.3 ns | 18.3 ns |
| get() path depth=4 | 588.5 ns | 299.9 ns |
| filter() path depth=4 | 1.82 µs | 780.4 ns |
| direct index depth=8 | 209.9 ns | 25.4 ns |
| get() path depth=8 | 864.1 ns | 455.1 ns |
| filter() path depth=8 | 3.25 µs | 1.39 µs |
| wrap('payload') depth=4 | 833.9 ns | 274.2 ns |
| unwrap('payload') depth=4 | 648.4 ns | 208.1 ns |

`get()` on an already-converted table costs ~0.1 µs per path segment plus
~0.25 µs fixed (PUC); direct indexing is ~20 ns/segment. Both are negligible
compared to converting the tree, which is why the lazy view replaces the
conversion rather than the indexing helper.

## 5. Lazy message view (engine implementation)

A message that reaches a Lua listener is a `LazyNode` userdata over the retained
`QVariant`; nothing is converted until a field is read. Columns:

- `eager convert` — full `QVariant -> Lua` of the same message (`DeepCached`);
- `lazy notify` — the real notification with nobody reading it (view creation);
- `lazy notify + read` — the notification plus walking `a.a…[1]` to the leaf;
- `eager convert + read` — the same leaf read out of the fully converted table;
- `read leaf (view)` — the leaf walk on an already captured view (no notify);
- `tostring` — JSON serialization of the whole view;
- `pairs` — iterate the root level;
- `forward view` — hand the view to another worker (`toQVar` returns the tree);
- `forward eager` — hand the same worker a converted Lua table instead;
- `materialize` — `view:materialize()` on an already materialized view.

All numbers are PUC Lua 5.4, µs unless noted.

| nodes | eager convert | lazy notify | lazy + read | eager + read | read leaf (view) | tostring | pairs | forward view | forward eager | materialize |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 1.42 | 4.05 | 5.51 | 1.46 | 1.14 | 1.72 | 1.05 | 386.9 ns | 1.23 | 323.5 ns |
| 3 | 2.06 | 4.13 | 6.29 | 2.10 | 1.90 | 2.44 | 1.04 | 372.2 ns | 1.86 | 323.4 ns |
| 4 | 2.60 | 4.03 | 6.96 | 2.69 | 2.68 | 3.09 | 1.08 | 377.6 ns | 2.53 | 325.5 ns |
| 5 | 3.15 | 3.99 | 8.05 | 3.31 | 3.55 | 3.74 | 1.09 | 381.9 ns | 3.19 | 325.4 ns |
| 7 | 4.31 | 4.01 | 9.47 | 4.51 | 5.06 | 5.02 | 1.05 | 373.3 ns | 4.46 | 321.9 ns |
| 16 | 3.77 | 4.03 | 5.52 | 3.83 | 1.24 | 6.11 | 4.57 | 378.3 ns | 8.23 | 324.3 ns |
| 24 | 6.33 | 4.01 | 6.34 | 6.38 | 2.14 | 9.49 | 4.48 | 374.7 ns | 14.40 | 326.8 ns |
| 128 | 21.94 | 4.03 | 5.71 | 22.22 | 1.37 | 34.79 | 39.52 | 377.9 ns | 67.19 | 325.2 ns |
| 96 | 20.13 | 3.98 | 6.50 | 20.91 | 2.22 | 31.47 | 18.65 | 375.4 ns | 61.41 | 325.1 ns |
| 64 | 16.08 | 4.01 | 7.72 | 16.28 | 3.03 | 23.45 | 8.91 | 373.1 ns | 42.58 | 325.1 ns |

What the table says:

- **Notification is O(1).** `lazy notify` is 4.0 µs for every message shape; that
  is the pipeline machinery (fiber entry, `call_all`, Lua handler), not the
  message. Creating the view itself is below the measurement noise (< 100 ns).
- **The eager path's notification would be flat + conversion.** `lazy + read`
  includes the 4 µs floor that `eager + read` does not, so the comparison
  understates the win: at 128 nodes an eager notification would cost
  ~4 + 22 = 26 µs against 5.7 µs — **~4.5x**. At 2-7 nodes both are ~floor, i.e.
  parity: small messages neither win nor lose.
- **Walking a view is ~0.8 µs per level** (`read leaf (view)`: 1.14 µs for one
  level, 5.06 µs for six); a converted table indexes in ~0.02 µs/level. A view
  should be read a bounded number of times, or materialized once.
- **Forwarding is the big win: ~375 ns flat**, versus 1.2 µs (2 nodes) to
  67 µs (128 nodes) for a Lua table — up to **180x**, and independent of shape.
  This is what the old eager pipeline paid at every hop.
- **`tostring` serializes, it does not preview**: JSON of the whole message
  (34.8 µs at 128 nodes, more than converting it — `QJsonDocument` build plus
  the conversion). Used for logging, so it is exact.
- **`pairs` over the root is O(width)**: 64 keys cost 39.5 µs because every
  value becomes a child view. Iterating a wide root is the one pattern where a
  view is expensive; `ipairs` over a list, `#`, and field reads are O(1).
- **`materialize` is free after the first call** (325 ns: a cache lookup). The
  first call pays the full conversion shown in the `eager convert` column; the
  benchmark's iterations amortize it away.

`build-jit` reproduces the same shape: notification 2.4 µs, forwarding
**~168 ns** flat, `materialize` ~106 ns, `tostring` 33.5 µs and `pairs` 41.0 µs
at 128 nodes.

### Forwarding through real workers (`bench_conversion.lua`)

Same message (depth 2, width 4, 12 nodes), the sender builds the Lua table once:

| case | PUC | LuaJIT |
|---|---:|---:|
| `Echo` a Lua table: Lua -> QVariant + notify | 12.34 µs | 9.22 µs |
| …its listener forwards the view to another worker | 12.80 µs | 9.32 µs |
| …the listener materializes first, then forwards | 24.44 µs | 19.13 µs |
| …two workers forward the view | 13.52 µs | 9.51 µs |

A forwarded view costs **~0.4-0.7 µs per hop**; a materialized hop costs
**+12 µs** (a full conversion down and back up). That difference is the entire
eager-pipeline tax the lazy view removes.

## Takeaways

1. **Message shape is the cost driver for conversion, not for dispatch.** Deep
   and wide messages are free inside Lua (reference passing) and linear per node
   at every C++ boundary that converts them.
2. **The lazy view removes the per-hop conversion tax.** Hops are now ~5.2 µs
   flat regardless of depth (was 6.4-15.7 µs, growing with depth); forwarding a
   view is ~375 ns instead of up to 67 µs.
3. **Reading through a view costs ~0.8 µs/level**, so views pay off when a
   handler reads a small part of a wide/deep message or forwards it, and break
   even on tiny messages (both are dominated by the 4 µs notification floor).
4. **`tostring` is exact (JSON) and costs like a conversion** — do not call it
   per message in a hot path just to log a count.
5. **Iterating a wide root with `pairs` is O(width) with child-view creation**;
   prefer reading known fields, or `materialize()` once if the handler really
   needs a table.
