# Lazy message indexing in Lua

Status: **implemented** (2026-10-21). Grounded in [`RESULTS.md`](RESULTS.md);
code in [`src/lazy.cpp`](../src/lazy.cpp), [`src/lazy.hpp`](../src/lazy.hpp),
tested by [`tests/lazy.lua`](../tests/lazy.lua) and
[`tests/scada_runner.lua`](../tests/scada_runner.lua).

## Problem

A message crossing a worker boundary used to be converted **eagerly and
completely**, in both directions:

- inbound (`worker_notify`, `src/worker.cpp`): `glua::Push(L, QVariant)` built a
  Lua table for every map and list in the tree;
- outbound (`toQVar`, `src/builtin.cpp`): every Lua table was copied back into
  `QVariant`.

Handlers rarely need the whole tree. A typical handler reads one path
(`msg.payload.value`) or just forwards the message to the next worker. Measured
on PUC Lua 5.4 (`RESULTS.md` §2, §3):

| cost | depth 1 | depth 8 | 128 nodes |
|---|---:|---:|---:|
| `QVariant` -> Lua table | 1.5 µs | 4.5 µs | 24.4 µs |
| Lua table -> `QVariant` | 1.9 µs | 5.3 µs | 74.3 µs |
| one C++ worker hop before lazy views | 6.1 µs | 15.0 µs | — |
| pure Lua hop (for reference) | 0.64 µs | 0.64 µs | 0.64 µs |

Marginal per-node conversion is **~0.62 µs/node Lua -> QVariant** and
**~0.19 µs/node QVariant -> Lua**. The conversion was paid *even when nothing
read the message* — exactly what `pipe(ingest, decode, logic)` does at every hop.

## What ships

A message that arrives in a Lua listener is a `LazyNode` userdata holding the
retained `QVariant` subtree. Scalars (`number`, `string`, `bool`, bytes, …)
cross as themselves; only maps and lists become views:

```lua
pipe(worker, function(msg)          -- msg is a LazyNode
    msg.payload.value               -- converts only this scalar
    msg.a.b.list[2]                 -- each container level yields a child view
    #msg.items                      -- __len over the retained list
    for k, v in pairs(msg) do end   -- O(1) per step, values are views/scalars
    tostring(msg)                   -- exact JSON of the whole message
    get(msg, "a:b")                 -- walks the QVariant, no materialization
    msg.a.b = 1                     -- write: the message becomes a table
    msg:materialize()               -- plain Lua table of the message
    other_worker(msg)               -- forwarding: the retained tree, zero copy
end)
```

### Semantics

1. **`__index`** — a string key indexes a map, a number an integer list index
   (1-based); anything else yields `nil`. Scalars are pushed eagerly, containers
   become child views (O(1): a `QVariant` copy plus a userdata). The `materialize`
   method is resolved before the key lookup.
2. **No child cache.** The prototype cached converted children per node; the
   engine does not, because a child never outlives the access unless the script
   keeps it, and a Lua table per node would cost memory in every message. Repeated
   access is cheap enough at ~0.6 µs/level (PUC); a handler that reads the same
   subtree repeatedly calls `materialize()` once.
3. **Copy-on-write of the whole message (`__newindex`).** The views of one
   message share an owner: the retained `QVariant` plus, once anything writes
   through *any* of them, the plain Lua table the whole message became (one deep
   conversion — exactly what the old eager delivery paid). A write materializes
   that table and sets the key at the view's own place in it, so
   `msg.nested.flag = 1` lands in the message, the way it did when a message
   arrived as a plain table, and every other view of the message sees it. Once
   the message is a table, reads through any view resolve into it, so
   `msg.nested` is then a plain table and no child views are created any more.
   A view's place is the chain of keys it was indexed with, kept in its user
   values (1 = the view it came from, 2 = the key it was indexed with); LuaJIT
   keeps the same two in the userdata environment.
4. **`__len`** is the retained list's length (`0` for maps, as in Lua) — or the
   length of the message's table once it was written to.
5. **`pairs` / `ipairs`** iterate the retained `QVariant` with an engine iterator
   (`next()`-style, O(1) per step), yielding scalars and child views. IPC maps are
   `QVariantMap`, i.e. key-sorted; `ipairs` over a map is empty, as usual. Lua 5.4
   drives this through `__pairs`, and its `ipairs` reaches the view through
   `__index`. Lua 5.1/LuaJIT honours neither metamethod, so there `Instance`
   wraps the `pairs`/`ipairs` globals: a view gets the engine iterator, every
   other value is delegated to the original function (the `#ifdef RADAPTER_JIT`
   branch; 5.4 keeps the stock globals). Once the message is a table (after any
   write, including one made inside the loop) iteration hands over to
   `next`/`ipairs` on it.
6. **`__tostring`** serializes the whole message as compact JSON (binary leaves
   as their `reprBinaries` form) — exact output, not a preview. It costs about
   as much as a conversion, so it belongs in logging, not in a hot loop.
7. **`toQVar(view)`** returns the retained `QVariant` (or the materialized table
   after a write). Forwarding a received message to another worker is therefore
   zero-copy: no Lua table is built and none is walked.
8. **`get(view, path)`** walks the `QVariant` directly, returning a child view or
   a scalar (so `get(msg, "payload")` is still cheap); `get(view, "")` is the view
   itself, the way `get(table, "")` is the table. `set(view, path, value)` puts the
   message's own table at the view's place and assigns into it, so
   `set(msg, "a:b", 1)` and `set(msg.a, "b", 1)` are visible to later `msg.a.b`
   reads and to a forwarded `msg`, exactly like `set(table, …)` on a table you
   own — and the returned table *is* the message's. `x:materialize()` on any view
   returns the message's own table at that path (writing into it writes the
   message), or the whole message table for the message view.
9. **`json_encode`** goes through `toQVar`, so it serializes the retained tree
   without building a Lua table. `fmt`/`log` interpolate a view through
   `__tostring`, i.e. as its JSON.
10. Lifetime: the message root's userdata owns the message (the retained tree,
    embedded, and the materialized table in its third user value) and destroys
    the tree in `__gc`; a derived view keeps the view it came from in its first
    user value, so a live view holds its whole chain up to the root. Nothing is
    copied until `materialize`/a write, and nothing is materialized merely by
    reading (a read of a pristine view allocates no Lua table). The table is a
    user value rather than a registry reference on purpose: a message that holds
    a view of itself (`msg.self = msg`) is then one collectable cycle, not a
    table pinned for the lifetime of the Lua state.
11. **Tag events** reach their listeners the same way (`tags.changed` and
    `tags.changed["name"]`), so the same rules apply to `ev.name`, `ev.value`,
    `ev.quality` and `ev.ts`.

### Rejected in review: a native worker -> worker pipe fast path

An earlier proposal (option A) suggested teaching `pipe(a, b)` to connect two
workers without a Lua closure. It is not implemented and is **not needed**: the
pipe stays pure Lua, and the zero-copy `toQVar` already makes a forwarded hop
cost the Lua call plus a reference copy (~305 ns vs up to 71 µs, `RESULTS.md`
§5). Adding a second, parallel way to connect workers would duplicate the
semantics `pipe`/`on`/`unpack` already provide.

## Portability

- Lua 5.4: `lua_newuserdatauv` + `lua_getiuservalue`/`lua_setiuservalue`,
  `luaL_len`; LuaJIT/5.1: `lua_newuserdata` + `lua_getfenv`/`lua_setfenv`,
  `lua_objlen`. Both paths live behind `RADAPTER_JIT` in `src/lazy.cpp`.
- A fresh LuaJIT userdata inherits the globals as its environment, so every view
  gets its own environment table (holding the parent view and the key).
- `compat::lua_absindex` covers the missing 5.1 `lua_absindex`.

## Compatibility and risks

- **`type(msg)` is `"userdata"`, not `"table"`.** Code checking
  `type(msg) == "table"` (or using `table.insert`, `table.sort`, … on a message)
  must call `msg:materialize()` first. Messages remain `table` wherever a script
  builds one.
- **Equality** is identity (no `__eq`): a view is never structurally equal to a
  table. Compare materialized values.
- **Iterating a wide root with `pairs` is O(width)** and creates a view per
  value (43 µs for 64 keys; `RESULTS.md` §5). Reading known fields is O(1).
- **`tostring` is a full serialization** — cheap to call once, expensive per
  message in a hot path.
- **The first write to any view converts the whole message** (one deep
  conversion — the cost the eager delivery used to pay at every hop, now paid at
  most once, and only by handlers that mutate). From then on every view resolves
  into that table, so reads are plain-table speed.
- **`set`/`materialize` hand out the message's own table**, never a detached copy;
  the retained tree of the sender is still never modified, so a handler that edits
  and forwards sends a fresh tree.
- **A view whose path no longer holds a container raises** (`attempt to index a
  nil value`): after `local sub = msg.a; msg.a = 5`, `sub.b` fails instead of
  reading a detached old value. Re-read from the message.
- **Storing a view inside a message stores a view**: `msg.copy = msg.a` keeps the
  userdata, so converting the message serializes the view's content while the
  entry stays a view (and `msg.self = msg` is the cycle below). Such a message
  stays collectable — the table lives in the root's user value, not in the
  registry (a 200-field message written with `msg.self = msg` 2000 times grows
  RSS exactly like a scalar write).
- **Table-only code needs `materialize()`.** A subtree taken off a message is a
  view, so anything that asserts `type(x) == "table"`, uses `table.*`, or walks
  the tree with its own helpers must be handed `x:materialize()`. `projects/scada`
  materializes the config it receives before `build()`/`save_to()`, and
  `tests/scada_runner.lua` guards that path.
- **A self-referential view raises.** `msg.self = msg` (or any cycle through a
  materialized view) makes conversion report *"lazy view conversion nests too
  deeply (does a message refer to itself?)"* instead of recursing into the C
  stack; the message stays readable afterwards.
- **QVariant types without a Lua equivalent** are converted exactly as before at
  the leaf; a view never changes leaf representation.
- Every entry into Lua already runs on a fiber with its own Lua thread, so views
  are subject to the same lifetime rules as any registry-held value.

## Verification

- `tests/lazy.lua` (registered in ctest) exercises field access, nested views,
  list indexing, `#`, `pairs`/`ipairs` (on a pristine view **and** after
  `materialize()` and after a write), JSON `tostring`, `json_encode`, `get`
  (including list path segments, misses, and `get(view, "")`), `materialize`
  (cached, same table, the message's own tables), writes through a nested view
  and two levels down (visible to later reads, to views captured before the
  write, to `pairs` including a write made inside the loop, to `tostring` and to
  the forwarded copy), `set` on the message and on a nested view, a
  self-referential view (`tostring`/`json_encode`/forwarding raise, the message
  stays readable), `wrap`/`unwrap`, forwarding a view to a worker that sends it
  back, and a message whose **root is a list** (`#`, indices, `ipairs`, `pairs`,
  `get`, `tostring`, forwarding).
- `tests/scada_runner.lua` spawns `projects/scada/scada.lua run-stdin` and
  asserts it builds the config it receives (the table-only-code path above).
- Full `ctest` passes in both `build` (PUC 5.4) and `build-jit` (LuaJIT), 18/18,
  including `tests/stdio.lua`, whose handler mutates the message.
- Memory: 300 000 notified messages, each read with `pairs`, mutated and
  `tostring`-ed, keep RSS flat (39 MB PUC / 26 MB JIT; Lua heap returns to
  ~84 KB after `collectgarbage`). The earlier runaway was a bug in the first
  `pairs` iterator (it built a key table per call); the shipped iterator walks
  the `QVariant` with a `next()`-style control value.

## Measured outcome

From `RESULTS.md`, PUC Lua 5.4:

| case | before (eager) | with the lazy view | |
|---|---:|---:|---|
| 1 worker hop, depth 1 | 6.1 µs | 5.6 µs | — |
| 1 worker hop, depth 8 | 15.0 µs | 11.2 µs | 1.3x |
| marginal hop, depth 1 -> 8 | 6.4 -> 15.7 µs | 4.9 -> 4.7 µs | flat |
| 3 hops, depth 8 | 46 µs | 20.5 µs | 2.2x |
| forward one 128-node message | 71 µs | 0.30 µs | ~230x |
| notify + read one leaf, 128 nodes | ~28 µs | 5.5 µs | ~5x |
| notify + read one leaf, 2 nodes | ~5.5 µs | 5.2 µs | parity |
| `tostring` (JSON) of 128 nodes | — | 35.2 µs | exact, not free |
| `pairs` over a 64-key root | — | 43 µs | O(width) |
