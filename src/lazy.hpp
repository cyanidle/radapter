#pragma once

#include "radapter/common.hpp"
#include "radapter/lazy.hpp"

struct lua_State;

namespace radapter::lazy {

/// Registers the lazy-node metatable and installs the `pairs`/`ipairs` wrappers
/// that know about lazy views (LuaJIT 5.1 does not honour `__pairs`).
void Init(lua_State* L);

bool IsLazy(lua_State* L, int idx);

/// The retained tree of the lazy node at `idx`, if it is one.
bool ToQVariant(lua_State* L, int idx, QVariant& out);

/// Pushes `v` as a lazy node (no materialization). The node is a message root:
/// it is allowed to take writes, which materialize it in place.
void Push(lua_State* L, QVariant const& v);

/// Pushes a child of a lazy node: containers become nested lazy nodes, scalars
/// are pushed eagerly. Child nodes reject writes — they are derived views and
/// cannot propagate an assignment back to the message.
void PushChild(lua_State* L, QVariant const& v);

/// Replaces the view at `idx` with its plain Lua table (materialized on first
/// use), so a table-only builtin mutates the view's own copy.
void MaterializeInPlace(lua_State* L, int idx);

/// `get(view, "deep:nested:key"[, sep])` on a lazy node.
int GetPath(lua_State* L);

}
