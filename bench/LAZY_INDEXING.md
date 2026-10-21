# Lazy message indexing in Lua

Status: **implemented** (2026-10-21). Grounded in [`RESULTS.md`](RESULTS.md);
code in [`src/lazy.cpp`](../src/lazy.cpp), [`include/radapter/lazy.hpp`](../include/radapter/lazy.hpp),
tested by [`tests/lazy.lua`](../tests/lazy.lua).

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
| `QVariant` -> Lua table | 1.4 µs | 4.3 µs | 21.9 µs |
| Lua table -> `QVariant` | 1.8 µs | 5.1 µs | 67.1 µs |
| one C++ worker hop before lazy views | 6.1 µs | 15.0 µs | — |
| pure Lua hop (for reference) | 0.70 µs | 0.70 µs | 0.70 µs |

Marginal per-node conversion is **~0.58 µs/node Lua -> QVariant** and
**~0.18 µs/node QVariant -> Lua**. The conversion was paid *even when nothing
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
    msg:materialize()               -- plain Lua table, cached on the view
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
   access is cheap enough at ~0.8 µs/level (PUC); a handler that reads the same
   subtree repeatedly calls `materialize()` once.
3. **Copy-on-write (`__newindex`).** A view is only the immutable source of
   truth. The first write to a **message view** converts that level into a plain
   Lua table stored in the userdata (user value on 5.4, environment on
   LuaJIT/5.1) and all metamethods prefer it afterwards — `msg.pong = 2` works
   and every later read, `pairs`, `tostring` and forward sees it. A view produced
   by *indexing* (`msg.nested`) is a derived temporary that cannot propagate an
   assignment to the message, so writing it raises instead of dropping the value
   silently: `msg.nested.flag = 1` → *"cannot write to a nested lazy message
   view; call materialize() on the message first or use set(msg, 'path',
   value)"*. After `msg:materialize()` (or a write to the message itself) the
   nested values are plain tables and nested assignment works normally.
4. **`__len`** is the retained list's length (`0` for maps, as in Lua).
5. **`pairs` / `ipairs`** iterate the retained `QVariant` with an engine iterator
   (`next()`-style, O(1) per step), yielding scalars and child views. IPC maps are
   `QVariantMap`, i.e. key-sorted; `ipairs` over a map is empty, as usual. Lua 5.4
   drives this through `__pairs`, and its `ipairs` reaches the view through
   `__index`. Lua 5.1/LuaJIT honours neither metamethod, so there `Instance`
   wraps the `pairs`/`ipairs` globals: a view gets the engine iterator, every
   other value is delegated to the original function (the `#ifdef RADAPTER_JIT`
   branch; 5.4 keeps the stock globals).
6. **`__tostring`** serializes the whole message as compact JSON (binary leaves
   as their `reprBinaries` form) — exact output, not a preview. It costs about
   as much as a conversion, so it belongs in logging, not in a hot loop.
7. **`toQVar(view)`** returns the retained `QVariant` (or the materialized table
   after a write). Forwarding a received message to another worker is therefore
   zero-copy: no Lua table is built and none is walked.
8. **`get(view, path)`** walks the `QVariant` directly, returning a child view or
   a scalar (so `get(msg, "payload")` is still cheap). `set(view, path, value)`
   materializes the view **in place** — the view's own copy becomes a plain
   table, the assignment lands in it, and the returned table *is* that copy — so
   `set(msg, "a:b", 1)` is visible to later `msg.a.b` reads and to a forwarded
   `msg`, exactly like `set(table, …)` on a table you own.
9. **`json_encode`** goes through `toQVar`, so it serializes the retained tree
   without building a Lua table. `fmt`/`log` interpolate a view through
   `__tostring`, i.e. as its JSON.
10. Lifetime: the userdata owns one `QVariant` copy (implicitly shared with the
    signal argument) and releases it in `__gc`. Child views share the same tree;
    nothing is copied until `materialize`/a write.

### Rejected in review: a native worker -> worker pipe fast path

An earlier proposal (option A) suggested teaching `pipe(a, b)` to connect two
workers without a Lua closure. It is not implemented and is **not needed**: the
pipe stays pure Lua, and the zero-copy `toQVar` already makes a forwarded hop
cost the Lua call plus a reference copy (~375 ns vs up to 67 µs, `RESULTS.md`
§5). Adding a second, parallel way to connect workers would duplicate the
semantics `pipe`/`on`/`unpack` already provide.

## Portability

- Lua 5.4: `lua_newuserdatauv` + `lua_getiuservalue`/`lua_setiuservalue`,
  `lua_rawlen`; LuaJIT/5.1: `lua_newuserdata` + `lua_getfenv`/`lua_setfenv`,
  `lua_objlen`. Both paths live behind `RADAPTER_JIT` in `src/lazy.cpp`.
- A fresh LuaJIT userdata inherits the globals as its environment, so
  `PushNode` installs an empty table there before the view can be written to.
- `compat::lua_absindex` covers the missing 5.1 `lua_absindex`.

## Compatibility and risks

- **`type(msg)` is `"userdata"`, not `"table"`.** Code checking
  `type(msg) == "table"` (or using `table.insert`, `table.sort`, … on a message)
  must call `msg:materialize()` first. Messages remain `table` wherever a script
  builds one.
- **Equality** is identity (no `__eq`): a view is never structurally equal to a
  table. Compare materialized values.
- **Iterating a wide root with `pairs` is O(width)** and creates a view per
  value (39.5 µs for 64 keys; `RESULTS.md` §5). Reading known fields is O(1).
- **`tostring` is a full serialization** — cheap to call once, expensive per
  message in a hot path.
- **Writing to a message view materializes that level**, so a mutation loop over
  a big message converts it; from then on the view *is* that plain table and the
  retained tree is no longer shared. **Nested views cannot take writes** (rule 3)
  — materialize the message, or use `set(msg, "path", value)`.
- **`set(view, path, value)` mutates the view's copy**; the retained tree of the
  sender is never modified, so a handler that edits and forwards sends a fresh
  tree.
- **QVariant types without a Lua equivalent** are converted exactly as before at
  the leaf; a view never changes leaf representation.
- Every entry into Lua already runs on a fiber with its own Lua thread, so views
  are subject to the same lifetime rules as any registry-held value.

## Verification

- `tests/lazy.lua` (registered in ctest) exercises field access, nested views,
  list indexing, `#`, `pairs`/`ipairs` (on a pristine view **and** after
  `materialize()` and after a write), JSON `tostring`, `json_encode`, `get`
  (including list path segments and misses), `materialize` (cached, same table),
  a root write (visible to reads, `pairs`, `tostring` and the forwarded copy),
  the nested-write error, `set` (in-place, visible to the view), `wrap`/`unwrap`
  and forwarding a view to a worker that sends it back.
- Full `ctest` passes in both `build` (PUC 5.4) and `build-jit` (LuaJIT), 17/17,
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
| 1 worker hop, depth 8 | 15.0 µs | 10.8 µs | 1.4x |
| marginal hop, depth 1 -> 8 | 6.4 -> 15.7 µs | 5.2 -> 5.0 µs | flat |
| 3 hops, depth 8 | 46 µs | 20.8 µs | 2.2x |
| forward one 128-node message | 67 µs | 0.38 µs | ~180x |
| notify + read one leaf, 128 nodes | ~26 µs | 5.7 µs | ~4.5x |
| notify + read one leaf, 2 nodes | ~5.4 µs | 5.5 µs | parity |
| `tostring` (JSON) of 128 nodes | — | 34.8 µs | exact, not free |
| `pairs` over a 64-key root | — | 39.5 µs | O(width) |
