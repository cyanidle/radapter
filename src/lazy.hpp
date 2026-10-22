#pragma once

#include "radapter/common.hpp"
#include <QVariant>

struct lua_State;

namespace radapter {

/// A message tree kept as QVariant until Lua actually reads it. `glua::Push`
/// turns it into a lazy view userdata instead of materializing tables; `toQVar`
/// returns the retained tree unchanged, so forwarding a message is free.
struct LazyNode {
    QVariant value;
};

}

Q_DECLARE_METATYPE(radapter::LazyNode)

namespace radapter::lazy {

/// Registers the lazy-node metatable and installs the `pairs`/`ipairs` wrappers
/// that know about lazy views (LuaJIT 5.1 does not honour `__pairs`).
void Init(lua_State* L);

bool IsLazy(lua_State* L, int idx);

/// The retained tree of the lazy node at `idx`, if it is one.
bool ToQVariant(lua_State* L, int idx, QVariant& out);

/// Pushes `v` as a lazy node (no materialization).
void Push(lua_State* L, QVariant const& v);

/// Replaces the view at `idx` with the plain Lua value it stands for: the
/// message is materialized (all views of it resolve through one table from then
/// on) and `idx` becomes the table at the view's place in it, so a table-only
/// builtin mutates the message.
void MaterializeInPlace(lua_State* L, int idx);

/// `get(view, "deep:nested:key"[, sep])` on a lazy node.
int GetPath(lua_State* L);

}
