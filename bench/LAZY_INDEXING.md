# Proposal: lazy message indexing for Lua

Status: proposal + measured prototype. Grounded in
[`RESULTS.md`](RESULTS.md); prototype in [`bench_plugin.cpp`](bench_plugin.cpp)
(`Bench:LazyView`, `Bench:LazyViewCached`).

## Problem

A message crossing a worker boundary is converted **eagerly and completely**:

- inbound (`worker_notify`, `src/worker.cpp`): `glua::Push(L, QVariant)` builds a
  Lua table for every map and list in the tree;
- outbound (`toQVar`, `src/builtin.cpp`): every Lua table is copied into
  `QVariant` again.

Handlers rarely need the whole tree. A typical handler reads one path
(`msg.payload.value`), or forwards the message to the next worker. Measured on
PUC Lua 5.4 (see `RESULTS.md`):

| cost | depth 1 | depth 8 | 128 nodes |
|---|---:|---:|---:|
| Lua table -> `QVariant` | 1.8 µs | 5.1 µs | 68 µs |
| `QVariant` -> Lua table | 1.4 µs | 4.5 µs | 22 µs |
| one C++ worker hop (both ways + dispatch) | 6.1 µs | 15.0 µs | — |
| pure Lua hop (for comparison) | 0.63 µs | 0.63 µs | 0.63 µs |

Marginal per-node conversion is **~0.58 µs/node Lua -> QVariant** and
**~0.18 µs/node QVariant -> Lua**. A 3-hop pipeline carrying depth-8 messages
spends ~30 µs of its 46 µs converting. Reading one field from an already
converted 100-node message costs 0.5-1 µs; converting it cost 20-80 µs.

The conversion is paid **even when nothing reads the message**, which is exactly
what the common `pipe(ingest, decode, logic)` topology does at every hop.

## Proposal

Two complementary changes; the first is cheap and independent of the second.

### A. Native worker -> worker forwarding (zero conversions)

`pipe(a, b)` where both are workers currently goes through Lua: `a`'s listener
list gets a Lua closure that calls `b:__call(msg)`, so the message is converted
QVariant -> Lua on notify and Lua -> QVariant on the call.

Instead, `connect()` should detect a worker target and attach `b->OnMsg` as a
C++ connection to `a->SendMsg` (the machinery already exists: `Worker::SendMsg`
is a Qt signal consumed by `WorkerImpl` for Lua listeners). No Lua value is ever
created; the `QVariant` is shared by copy-on-write.

Measured current cost per hop: **6.4 µs (depth 1) to 15.7 µs (depth 8)**.
This option removes essentially all of it, independent of message shape, and
needs no new Lua-visible type. It does not help a Lua handler that reads a
field — that is option B.

### B. Lazy message views (what handlers see)

Instead of a plain table, an inbound notification can pass a **userdata view**
over the retained `QVariant` subtree:

```lua
-- msg is a view
msg.payload.value            -- converts only this scalar (map lookup, no table copy)
msg.a.a.a[1]                 -- each container level yields a child view (O(1))
local n = #msg.items         -- __len on the retained list
for k, v in pairs(msg) do    -- __pairs: materializes that level (documented)
msg:materialize()            -- explicit full conversion, plain Lua table
json_encode(msg)             -- builtins materialize internally
other_worker(msg)            -- toQVar fast-path: returns the retained QVariant (zero copy)
```

Design points:

1. **`__index` converts only the addressed child.** Scalars (`bool`, numbers,
   `QString`, bytes) are pushed directly; maps/lists produce another view.
2. **Per-node child cache.** Converted children are stored in a Lua table in the
   userdata's user value (Lua 5.4 `lua_setiuservalue`) or environment (LuaJIT
   5.1 `lua_setfenv`), so repeated access to the same field costs a table lookup.
   The prototype implements both modes.
3. **`toQVar` fast path.** A view carries its `QVariant`; converting it back is
   `return retained` — no tree walk. This makes every Lua-side forward free.
4. **`materialize()`** recursively builds plain Lua tables with the same rules as
   today's `glua::Push`; `json_encode`, `fmt`/logging and any code that needs a
   whole message call it (or the builtins do it internally).
5. **`get`/`set` accept views.** `get(view, "a:b:c")` walks the `QVariant`
   directly (no materialization). `set` materializes first: messages are
   immutable by convention between workers, and copy-on-write is the simple,
   predictable rule.
6. **`__tostring` prints a compact preview** (`lazy(QVariantMap, 7 keys)`) so
   logs and error messages stay readable.

## What the numbers say

Prototype measurements (PUC, `bench_indexing.lua`; all include the benchmark's
0.48 µs extra-method call floor):

| message | eager convert + read one leaf | lazy view + read one leaf | speedup |
|---|---:|---:|---:|
| depth=1 width=64 (128 nodes) | 22.3 µs | 2.1 µs | **10.7x** |
| depth=2 width=32 (96 nodes) | 21.0 µs | 2.7 µs | **7.8x** |
| depth=3 width=16 (64 nodes) | 16.2 µs | 3.3 µs | **4.9x** |
| depth=2 width=8 (24 nodes) | 6.6 µs | 2.8 µs | 2.3x |
| depth=6 width=1 (7 nodes) | 4.6 µs | 4.5 µs | 1.0x (parity) |
| depth=1 width=1 (2 nodes) | 1.4 µs | 2.0 µs | 0.7x (loss) |

- View creation is **O(1)**: 1.08-1.15 µs regardless of message size (the floor
  is the call/`QVariant` plumbing, not the view).
- Walking to a leaf costs **~0.5 µs per level uncached** and **~0.2 µs per
  level cached**; direct table indexing is ~0.02-0.15 µs per level. The cached
  mode is what repeated field access in a handler would use.
- Break-even for a one-shot read of depth `d` against a message of `N` nodes:
  `0.18·N > 0.5·d − 1.1` (using the two conversion slopes and the lazy per-level
  cost). For `d = 6` that is `N > ~11` nodes. Messages wider than they are deep
  (the common case for device telemetry, database rows, SCADA frames) win
  immediately; narrow chains only win when they are also forwarded.
- Forwarding (option A / `toQVar` fast path) removes **6.4-15.7 µs per hop**,
  i.e. 10-25x the current hop cost, regardless of shape.

## Integration plan

1. **Infrastructure**: `LazyValue` userdata in the SDK (`include/radapter/`),
   `pushLazy(QVariant)` + `materialize`, registered as a metatable; `toQVar`
   recognizes it. No behaviour change yet.
2. **Forwarding (A)**: teach `connect()` in `src/scripts/builtins.lua` (or the
   C++ side of `pipe`) to bind worker targets without a Lua closure. This is
   invisible to scripts and gives the largest single win.
3. **Opt-in views (B)**: add `lazy_msgs` to `WorkerConfig` (or a
   `--lazy-msgs` instance flag) and pass views in `worker_notify`. Covered by
   `tests/smoke.lua`, `tests/basic.lua` and a new test that exercises
   `get`/`materialize`/`json_encode`/forwarding on a view.
4. **Default on**: after the tests and the examples pass, make views the default
   for workers that opt in via their schema and document the `type()` change.

## Compatibility and risks

- **`type(msg)` changes** from `"table"` to `"userdata"`. Code doing
  `type(msg) == "table"` or `msg == other` must be audited (`grep` in
  `examples/`, `projects/`, `definitions.lua`). `__eq` can compare views by
  retained `QVariant`, but table-vs-view equality cannot be structural both
  ways; document "compare materialized values".
- **`pairs`/`ipairs`/`#`**: Lua 5.4 honours `__pairs`; LuaJIT 5.1 does **not**
  (it needs a `pairs` wrapper). Since the shipped cart stack is LuaJIT, the
  design must either materialize on `pairs` via a global override or ship a
  helper. `#` maps to `__len` in both.
- **Every C API taking a table** needs an explicit decision: `get`/`set`, `merge`,
  `filter`, `json_encode`, `fmt`. The prototype only needed `get`; the plan is
  "walk lazily where the operation is read-only, materialize where the result is
  a new tree".
- **Mutation**: `set(view, ...)` materializes (copy-on-write). This keeps
  messages immutable across hops, which is already the convention.
- **QVariant types without a Lua equivalent** (`QObject*`, functions, custom
  types) are converted exactly as today at the leaf; a view never changes the
  leaf representation.
- **Lifetime**: a view owns a `QVariant` copy (implicitly shared with the
  signal argument), destroyed by `__gc`; child views share the same map data, so
  no tree is copied until `materialize`. Cross-fiber use is the same as any
  registry-held Lua value.
- **Not beneficial for**: narrow deep chains read once (parity at depth 6) and
  tiny messages (fixed overhead). Hence the opt-in rollout.

## Prototype

`bench/bench_plugin.cpp` contains the working prototype used for the numbers:

- `LazyNode { QVariant value; }` userdata, `__index` handling string keys,
  numeric list indices and the `materialize`/`type_name` methods;
- two metatables: plain and child-caching (cache in user value / environment);
- direct allocation (`lua_newuserdatauv` / `lua_newuserdata`) with a metatable
  cached in the registry — `LuaUserData::Create` is not on this path.

Side finding: `LuaUserData::Create` in the SDK set the metatable on the
metatable instead of the new userdata (`lua_setmetatable(L, -1)` where the
metatable was already on top). It had no callers; fixed in `src/value.cpp` while
building the prototype.
