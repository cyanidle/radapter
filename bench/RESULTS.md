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

## 1. Pipeline dispatch (pure Lua)

One `pipe()` hop is a Lua function call through `call_all`/`xpcall`; message
depth does not matter because Lua only passes the table reference. A hop that
stamps a field costs ~60% more than the pure dispatch floor.

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| stamp chain depth=1 hops=1 | 1.06 µs | 0.9 ns |
| stamp chain depth=1 hops=3 | 2.33 µs | 196.0 ns |
| stamp chain depth=1 hops=5 | 3.59 µs | 568.2 ns |
| stamp chain depth=2 hops=1 | 1.04 µs | 176.1 ns |
| stamp chain depth=2 hops=3 | 2.28 µs | 371.5 ns |
| stamp chain depth=2 hops=5 | 3.67 µs | 547.8 ns |
| stamp chain depth=4 hops=1 | 1.05 µs | 174.1 ns |
| stamp chain depth=4 hops=3 | 2.33 µs | 391.8 ns |
| stamp chain depth=4 hops=5 | 3.58 µs | 580.7 ns |
| stamp chain depth=8 hops=1 | 1.05 µs | 182.3 ns |
| stamp chain depth=8 hops=3 | 2.32 µs | 380.9 ns |
| stamp chain depth=8 hops=5 | 3.66 µs | 555.8 ns |
| unwrap/wrap hop depth=1 | 2.99 µs | 577.5 ns |
| wrap only depth=1 | 761.2 ns | 263.8 ns |
| unwrap only depth=1 | 595.0 ns | 197.2 ns |
| unwrap/wrap hop depth=4 | 3.00 µs | 630.6 ns |
| wrap only depth=4 | 775.4 ns | 263.6 ns |
| unwrap only depth=4 | 597.8 ns | 204.8 ns |
| filter() path depth=1 | 716.9 ns | 278.7 ns |
| filter() path depth=2 | 1.09 µs | 424.4 ns |
| filter() path depth=4 | 1.80 µs | 731.7 ns |
| filter() path depth=8 | 3.17 µs | 1.29 µs |

PUC: marginal dispatch is **~0.63 µs per hop**, independent of depth
(`hops=5 − hops=1` = 2.53 µs / 4). `wrap`/`unwrap` are O(1) (`set`/`get` on one
key), `filter()` is O(path depth) because it rebuilds the path.

> LuaJIT: the first `hops=1` row is trace-inlined to sub-nanosecond speed; later
> bytecode-identical benchmarks fall back to the interpreter (~175 ns). Do not
> compare LuaJIT rows of this section to each other; the PUC column is the
> stable reference.

## 2. C++ <-> Lua message conversion

`Lua -> QVariant` is `toQVar()` (recursive Lua table walk), `QVariant -> Lua`
is `glua::Push(QVariant)` (recursive table build). Both are eager and touch
every node of the message.

| case | Lua -> QVariant | QVariant -> Lua | roundtrip |
|---|---:|---:|---:|
| depth=1 width=1 nodes=2 | 1.82 µs | 1.38 µs | 2.75 µs |
| depth=2 width=1 nodes=3 | 2.53 µs | 1.97 µs | 4.02 µs |
| depth=3 width=1 nodes=4 | 3.13 µs | 2.50 µs | 5.35 µs |
| depth=4 width=1 nodes=5 | 3.93 µs | 3.22 µs | 6.66 µs |
| depth=6 width=1 nodes=7 | 5.05 µs | 4.48 µs | 9.02 µs |
| depth=1 width=8 nodes=16 | 9.10 µs | 3.95 µs | 12.62 µs |
| depth=2 width=8 nodes=24 | 15.57 µs | 6.57 µs | 21.15 µs |
| depth=1 width=64 nodes=128 | 68.30 µs | 22.35 µs | 93.22 µs |
| depth=2 width=32 nodes=96 | 63.92 µs | 21.36 µs | 83.90 µs |
| depth=3 width=16 nodes=64 | 44.09 µs | 16.72 µs | 60.21 µs |

Least-squares fit over all cases (PUC):

| direction | fixed cost | per node |
|---|---:|---:|
| Lua -> QVariant | 1.49 µs | 0.58 µs |
| QVariant -> Lua | 2.17 µs | 0.18 µs |
| roundtrip | 2.93 µs | 0.77 µs |

So a *single* deep message (depth 8, 9 nodes) costs **~10 µs** to cross one
boundary and back; a 100-node message costs **~80 µs**. Conversion dominates
everything else in a pipeline (a pure-Lua hop is 0.63 µs).

The extra-method call floor on the same worker is **0.48 µs** — conversion of
even a two-node message is already 3x that.

## 3. Pipeline across C++ worker boundaries

Each hop is: notify Lua (QVariant -> Lua, full tree) + call the next worker
(Lua -> QVariant, full tree).

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| depth=1 hops=1 | 6.08 µs | 4.01 µs |
| depth=1 hops=2 | 12.47 µs | 7.90 µs |
| depth=1 hops=3 | 18.96 µs | 12.24 µs |
| depth=2 hops=1 | 7.43 µs | 5.18 µs |
| depth=2 hops=2 | 15.13 µs | 10.10 µs |
| depth=2 hops=3 | 22.27 µs | 15.69 µs |
| depth=4 hops=1 | 9.96 µs | 7.35 µs |
| depth=4 hops=2 | 19.91 µs | 14.55 µs |
| depth=4 hops=3 | 30.21 µs | 22.04 µs |
| depth=8 hops=1 | 15.01 µs | 11.43 µs |
| depth=8 hops=2 | 29.92 µs | 22.28 µs |
| depth=8 hops=3 | 46.25 µs | 34.42 µs |

Marginal cost of one more hop: **6.4 µs at depth 1, 15.7 µs at depth 8** (PUC).
Depth adds ~1.3 µs per level per hop, i.e. Lua->QVariant + QVariant->Lua + 0.63 µs
of dispatch. Three hops at depth 8 cost 46 µs per message — 46 000 messages/s on
one core, *before* the handler does any work.

## 4. Path indexing in Lua

| case | PUC Lua 5.4 | LuaJIT |
|---|---:|---:|
| direct index depth=1 | 139.9 ns | 13.4 ns |
| get() path depth=1 | 350.0 ns | 157.3 ns |
| filter() path depth=1 | 698.0 ns | 273.1 ns |
| direct index depth=2 | 148.2 ns | 11.5 ns |
| get() path depth=2 | 412.2 ns | 206.0 ns |
| filter() path depth=2 | 1.05 µs | 412.7 ns |
| direct index depth=4 | 171.0 ns | 21.7 ns |
| get() path depth=4 | 565.2 ns | 279.6 ns |
| filter() path depth=4 | 1.72 µs | 727.3 ns |
| direct index depth=8 | 209.9 ns | 32.2 ns |
| get() path depth=8 | 832.5 ns | 425.0 ns |
| filter() path depth=8 | 3.13 µs | 1.29 µs |
| wrap('payload') depth=4 | 793.5 ns | 261.0 ns |
| unwrap('payload') depth=4 | 595.0 ns | 203.4 ns |

`get()` on an already-converted table costs ~0.11 µs per path segment plus
~0.24 µs fixed (PUC); direct indexing is ~20 ns/segment. Both are negligible
compared to converting the tree — reading one field out of a 100-node message
costs 0.5-1 µs, while converting that message costs 20-80 µs.

## 5. Lazy message view (prototype)

`eager convert` = full `QVariant -> Lua`; `lazy view` = create the view (no
conversion); `read leaf` = walk `a.a...a[1]`; `cached` = the same walk with
converted children cached in each node. Depth-1 `read` includes the leaf list
index. All numbers include the 0.48 µs extra-method call floor.

| case | eager convert | lazy view | eager + read | lazy + read | read eager | read lazy | read cached | materialize |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| depth=1 width=1 nodes=2 | 1.39 µs | 1.12 µs | 1.44 µs | 1.98 µs | 137.4 ns | 814.0 ns | 512.0 ns | 792.6 ns |
| depth=2 width=1 nodes=3 | 1.95 µs | 1.14 µs | 2.08 µs | 2.49 µs | 145.9 ns | 1.35 µs | 673.1 ns | 1.08 µs |
| depth=3 width=1 nodes=4 | 2.61 µs | 1.12 µs | 2.75 µs | 3.07 µs | 158.2 ns | 1.95 µs | 849.9 ns | 1.39 µs |
| depth=4 width=1 nodes=5 | 3.17 µs | 1.10 µs | 3.20 µs | 3.44 µs | 171.8 ns | 2.46 µs | 1.03 µs | 1.66 µs |
| depth=6 width=1 nodes=7 | 4.50 µs | 1.11 µs | 4.56 µs | 4.54 µs | 195.7 ns | 3.26 µs | 1.37 µs | 2.21 µs |
| depth=1 width=8 nodes=16 | 3.75 µs | 1.12 µs | 3.92 µs | 2.01 µs | 134.2 ns | 969.2 ns | 500.7 ns | 2.33 µs |
| depth=2 width=8 nodes=24 | 6.61 µs | 1.15 µs | 6.57 µs | 2.81 µs | 151.5 ns | 1.58 µs | 698.3 ns | 3.56 µs |
| depth=1 width=64 nodes=128 | 22.19 µs | 1.08 µs | 22.28 µs | 2.09 µs | 135.8 ns | 974.8 ns | 481.1 ns | 12.88 µs |
| depth=2 width=32 nodes=96 | 21.34 µs | 1.08 µs | 21.01 µs | 2.71 µs | 146.1 ns | 1.66 µs | 676.9 ns | 11.84 µs |
| depth=3 width=16 nodes=64 | 16.53 µs | 1.11 µs | 16.15 µs | 3.32 µs | 160.9 ns | 2.27 µs | 856.5 ns | 8.85 µs |

One-shot "read one leaf" speedup of lazy over eager conversion:

| case | eager / lazy |
|---|---:|
| depth=1 width=1 nodes=2 | 0.7x |
| depth=2 width=1 nodes=3 | 0.8x |
| depth=3 width=1 nodes=4 | 0.9x |
| depth=4 width=1 nodes=5 | 0.9x |
| depth=6 width=1 nodes=7 | 1.0x |
| depth=1 width=8 nodes=16 | 2.0x |
| depth=2 width=8 nodes=24 | 2.3x |
| depth=1 width=64 nodes=128 | 10.7x |
| depth=2 width=32 nodes=96 | 7.8x |
| depth=3 width=16 nodes=64 | 4.9x |

Lazy overhead per level: ~0.5 µs uncached, ~0.2 µs with the child cache
(PUC); direct table indexing is ~0.02 µs/level.

## Takeaways

1. **Message shape is the cost driver.** Deep/wide messages are cheap inside
   Lua (reference passing) but linear per node at every C++ boundary.
2. **Conversion is the pipeline tax.** ~0.58 µs/node Lua->QVariant and
   ~0.18 µs/node QVariant->Lua; a 3-hop pipeline at depth 8 spends ~30 µs of
   its 46 µs converting.
3. **Indexing is cheap, conversion is not.** `get()`/`filter()` cost 0.3-3 µs
   *after* the message was already converted; the same field accessed through
   a lazy view avoids 5-11x the conversion work on realistic wide messages,
   and the cached variant brings repeated access close to table speed.
4. **Deep narrow chains do not benefit** from lazy views (parity at depth 6);
   narrow messages also pay the fixed view/call overhead. Lazy indexing should
   be opt-in or applied where messages are wide, deep, or forwarded.
5. **Forwarding is the biggest opportunity**: every worker-to-worker hop pays
   two full conversions. A retained `QVariant` in a lazy view makes forwarding
   zero-copy; see `LAZY_INDEXING.md`.
