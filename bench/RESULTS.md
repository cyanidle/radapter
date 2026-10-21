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
| stamp chain depth=1 hops=1 | 1.07 µs | 154.0 ns |
| stamp chain depth=1 hops=3 | 2.38 µs | 337.5 ns |
| stamp chain depth=1 hops=5 | 3.61 µs | 621.7 ns |
| stamp chain depth=2 hops=1 | 1.06 µs | 173.3 ns |
| stamp chain depth=2 hops=3 | 2.35 µs | 394.9 ns |
| stamp chain depth=2 hops=5 | 3.63 µs | 625.2 ns |
| stamp chain depth=4 hops=1 | 1.08 µs | 174.2 ns |
| stamp chain depth=4 hops=3 | 2.34 µs | 388.7 ns |
| stamp chain depth=4 hops=5 | 3.66 µs | 622.8 ns |
| stamp chain depth=8 hops=1 | 1.08 µs | 176.5 ns |
| stamp chain depth=8 hops=3 | 2.36 µs | 390.5 ns |
| stamp chain depth=8 hops=5 | 3.64 µs | 638.1 ns |
| unwrap/wrap hop depth=1 | 3.04 µs | 677.9 ns |
| wrap only depth=1 | 790.8 ns | 263.9 ns |
| unwrap only depth=1 | 628.0 ns | 210.3 ns |
| unwrap/wrap hop depth=4 | 3.01 µs | 715.4 ns |
| wrap only depth=4 | 815.7 ns | 267.0 ns |
| unwrap only depth=4 | 624.4 ns | 219.4 ns |
| filter() path depth=1 | 763.7 ns | 300.2 ns |
| filter() path depth=2 | 1.12 µs | 448.2 ns |
| filter() path depth=4 | 1.93 µs | 779.9 ns |
| filter() path depth=8 | 3.25 µs | 1.42 µs |

PUC: marginal dispatch is **~0.64 µs per hop**, independent of depth
(`hops=5 − hops=1` = 2.55 µs / 4). `wrap`/`unwrap` are O(1) (`set`/`get` on one
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
| depth=1 width=1 nodes=2 | 1.92 µs | 1.46 µs | 2.80 µs |
| depth=2 width=1 nodes=3 | 2.63 µs | 2.07 µs | 4.12 µs |
| depth=3 width=1 nodes=4 | 3.33 µs | 2.71 µs | 5.51 µs |
| depth=4 width=1 nodes=5 | 4.00 µs | 3.34 µs | 6.76 µs |
| depth=6 width=1 nodes=7 | 5.32 µs | 4.53 µs | 9.46 µs |
| depth=1 width=8 nodes=16 | 9.26 µs | 4.08 µs | 12.90 µs |
| depth=2 width=8 nodes=24 | 16.07 µs | 7.01 µs | 23.22 µs |
| depth=1 width=64 nodes=128 | 74.31 µs | 24.39 µs | 101.87 µs |
| depth=2 width=32 nodes=96 | 67.40 µs | 22.49 µs | 93.67 µs |
| depth=3 width=16 nodes=64 | 46.16 µs | 17.12 µs | 64.30 µs |

LuaJIT, same cases:

| case | Lua -> QVariant | QVariant -> Lua | roundtrip |
|---|---:|---:|---:|
| nodes=2 | 1.54 µs | 1.03 µs | 2.19 µs |
| nodes=3 | 2.14 µs | 1.58 µs | 3.24 µs |
| nodes=4 | 2.70 µs | 2.02 µs | 4.56 µs |
| nodes=5 | 3.20 µs | 2.58 µs | 5.48 µs |
| nodes=7 | 4.31 µs | 3.51 µs | 7.55 µs |
| nodes=16 | 7.58 µs | 3.19 µs | 10.35 µs |
| nodes=24 | 13.06 µs | 5.57 µs | 18.52 µs |
| nodes=128 | 63.45 µs | 19.53 µs | 84.40 µs |
| nodes=96 | 59.67 µs | 19.13 µs | 79.73 µs |
| nodes=64 | 39.77 µs | 14.42 µs | 53.12 µs |

Least-squares fit over all cases:

| direction | PUC fixed | PUC per node | JIT fixed | JIT per node |
|---|---:|---:|---:|---:|
| Lua -> QVariant | 1.27 µs | 0.62 µs | 0.81 µs | 0.54 µs |
| QVariant -> Lua | 2.18 µs | 0.19 µs | 1.65 µs | 0.16 µs |
| roundtrip | 2.72 µs | 0.85 µs | 1.97 µs | 0.71 µs |

So a *single* deep message (depth 8, 9 nodes) costs **~9.5 µs** to cross one
boundary and back; a 100-node message costs **~95 µs**. Conversion dominates
everything else in a pipeline (a pure-Lua hop is 0.64 µs).

The extra-method call floor on the same worker is **0.54 µs** (PUC) / **0.37 µs**
(JIT) — conversion of even a two-node message is 3.5x that.

## 3. Pipeline across C++ worker boundaries

Each hop is a notification (message -> Lua listener, now a lazy view) plus a
call of the next worker (a view converts back to the retained tree for free).
Since neither direction walks the tree any more, a hop costs the same at every
depth:

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| depth=1 hops=1 | 5.76 µs | 3.79 µs |
| depth=1 hops=2 | 10.35 µs | 6.78 µs |
| depth=1 hops=3 | 15.65 µs | 9.68 µs |
| depth=2 hops=1 | 6.41 µs | 4.39 µs |
| depth=2 hops=2 | 11.20 µs | 7.41 µs |
| depth=2 hops=3 | 16.12 µs | 10.39 µs |
| depth=4 hops=1 | 8.09 µs | 5.63 µs |
| depth=4 hops=2 | 12.99 µs | 8.62 µs |
| depth=4 hops=3 | 18.04 µs | 11.64 µs |
| depth=8 hops=1 | 11.17 µs | 8.17 µs |
| depth=8 hops=2 | 15.73 µs | 11.32 µs |
| depth=8 hops=3 | 20.53 µs | 14.57 µs |

Marginal cost of one more hop is **~4.9 µs flat** (PUC) / **~3.0 µs** (JIT),
whereas before the lazy view it grew with the message: 6.4 µs at depth 1 and
15.7 µs at depth 8. Three hops at depth 8 cost 20.5 µs instead of 46 µs.

What remains is fixed per hop: fiber dispatch, `call_all`, the Lua handler and
the next worker call — not the message.

## 4. Path indexing in Lua

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| direct index depth=1 | 131.1 ns | 16.1 ns |
| get() path depth=1 | 393.4 ns | 180.2 ns |
| filter() path depth=1 | 763.7 ns | 300.2 ns |
| direct index depth=2 | 141.1 ns | 13.9 ns |
| get() path depth=2 | 477.5 ns | 218.1 ns |
| filter() path depth=2 | 1.12 µs | 448.2 ns |
| direct index depth=4 | 167.7 ns | 18.7 ns |
| get() path depth=4 | 610.9 ns | 316.0 ns |
| filter() path depth=4 | 1.93 µs | 779.9 ns |
| direct index depth=8 | 203.1 ns | 26.1 ns |
| get() path depth=8 | 900.4 ns | 481.9 ns |
| filter() path depth=8 | 3.25 µs | 1.42 µs |
| wrap('payload') depth=4 | 808.2 ns | 265.7 ns |
| unwrap('payload') depth=4 | 627.5 ns | 217.7 ns |

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
| 2 | 1.45 | 4.04 | 5.18 | 1.52 | 970 ns | 1.73 | 1.07 | 314 ns | 1.29 | 332 ns |
| 3 | 2.09 | 4.18 | 5.98 | 2.17 | 1.55 | 2.40 | 1.06 | 317 ns | 2.03 | 332 ns |
| 4 | 2.74 | 4.07 | 6.63 | 2.85 | 2.19 | 3.03 | 1.07 | 315 ns | 2.66 | 330 ns |
| 5 | 3.32 | 4.05 | 7.04 | 3.44 | 2.76 | 3.65 | 1.06 | 318 ns | 3.31 | 337 ns |
| 7 | 4.86 | 4.25 | 8.53 | 4.78 | 3.98 | 5.10 | 1.11 | 316 ns | 4.79 | 331 ns |
| 16 | 4.26 | 4.09 | 5.31 | 4.21 | 1.09 | 6.20 | 4.71 | 321 ns | 8.68 | 334 ns |
| 24 | 6.97 | 4.02 | 6.12 | 7.10 | 1.77 | 9.57 | 4.66 | 313 ns | 15.36 | 336 ns |
| 128 | 23.82 | 4.04 | 5.51 | 23.55 | 1.13 | 35.22 | 40.06 | 312 ns | 71.03 | 331 ns |
| 96 | 22.50 | 4.06 | 6.07 | 22.17 | 1.84 | 31.58 | 18.63 | 311 ns | 65.23 | 333 ns |
| 64 | 17.05 | 3.99 | 6.86 | 17.74 | 2.61 | 24.26 | 9.07 | 314 ns | 45.86 | 331 ns |

What the table says:

- **Notification is O(1).** `lazy notify` is ~4.0 µs for every message shape; that
  is the pipeline machinery (fiber entry, `call_all`, Lua handler), not the
  message. Creating the view itself is below the measurement noise (< 100 ns).
- **The eager path's notification would be flat + conversion.** `lazy + read`
  includes the 4 µs floor that `eager + read` does not, so the comparison
  understates the win: at 128 nodes an eager notification would cost
  ~4 + 24 = 28 µs against 5.5 µs — **~5x**. At 2-7 nodes both are ~floor, i.e.
  parity: small messages neither win nor lose.
- **Walking a view is ~0.6 µs per level** (`read leaf (view)`: 0.97 µs for one
  level, 3.98 µs for six); a converted table indexes in ~0.02 µs/level. A view
  should be read a bounded number of times, or materialized once.
- **Forwarding is the big win: ~315 ns flat**, versus 1.3 µs (2 nodes) to
  71 µs (128 nodes) for a Lua table — up to **~230x**, and independent of shape.
  This is what the old eager pipeline paid at every hop.
- **`tostring` serializes, it does not preview**: JSON of the whole message
  (35.2 µs at 128 nodes, more than converting it — `QJsonDocument` build plus
  the conversion). Used for logging, so it is exact.
- **`pairs` over the root is O(width)**: 64 keys cost 40.1 µs because every
  value becomes a child view. Iterating a wide root is the one pattern where a
  view is expensive; `ipairs` over a list, `#`, and field reads are O(1).
- **`materialize` is free after the first call** (~330 ns: a cache lookup). The
  first call pays the full conversion shown in the `eager convert` column; the
  benchmark's iterations amortize it away.

`build-jit` reproduces the same shape: notification 2.5 µs, forwarding
**~175 ns** flat, `materialize` ~108 ns, `tostring` 35.5 µs and `pairs` 43.2 µs
at 128 nodes.

### Forwarding through real workers (`bench_conversion.lua`)

Same message (depth 2, width 4, 12 nodes), the sender builds the Lua table once:

| case | PUC | LuaJIT |
|---|---:|---:|
| `Echo` a Lua table: Lua -> QVariant + notify | 12.59 µs | 9.56 µs |
| …its listener forwards the view to another worker | 12.90 µs | 9.73 µs |
| …the listener materializes first, then forwards | 24.89 µs | 19.92 µs |
| …two workers forward the view | 13.23 µs | 10.16 µs |

A forwarded view costs **~0.3-0.6 µs per hop**; a materialized hop costs
**+12 µs** (a full conversion down and back up). That difference is the entire
eager-pipeline tax the lazy view removes.

## Takeaways

1. **Message shape is the cost driver for conversion, not for dispatch.** Deep
   and wide messages are free inside Lua (reference passing) and linear per node
   at every C++ boundary that converts them.
2. **The lazy view removes the per-hop conversion tax.** Hops are now ~4.9 µs
   flat regardless of depth (was 6.4-15.7 µs, growing with depth); forwarding a
   view is ~315 ns instead of up to 71 µs.
3. **Reading through a view costs ~0.6 µs/level**, so views pay off when a
   handler reads a small part of a wide/deep message or forwards it, and break
   even on tiny messages (both are dominated by the 4 µs notification floor).
4. **`tostring` is exact (JSON) and costs like a conversion** — do not call it
   per message in a hot path just to log a count.
5. **Iterating a wide root with `pairs` is O(width) with child-view creation**;
   prefer reading known fields, or `materialize()` once if the handler really
   needs a table.
