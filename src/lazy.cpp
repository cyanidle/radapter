#include "lazy.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "glua/glua.hpp"
#include <QJsonDocument>

using namespace radapter;

namespace {

constexpr const char* LazyMeta = "radapter.LazyNode";

// What every view of one message shares: the retained tree and, once anything
// writes through any of the views, the plain Lua table the whole message became
// (kept in the root view's third user value, see PushMessageTable). A write
// through a nested view therefore lands in the message itself, the way it did
// when a message arrived as a plain table. The owner sits inside the message
// root's userdata, and every derived view keeps its parent alive, so the root's
// collector releases the owner.
struct LazyOwner {
    QVariant retained;
    bool materialized = false;
};

// A view is a position in its message: the retained subtree plus the chain of
// keys that leads to it from the message root, which is what an assignment
// needs once the message became a table.
struct LazyUserData {
    LazyOwner* owner;
    LazyUserData* parent; // null for the message root (which also embeds the owner)
    QVariant value;
};

static_assert(alignof(LazyOwner) <= alignof(LazyUserData));

constexpr size_t RootSize = sizeof(LazyUserData) + sizeof(LazyOwner);

LazyUserData* TestLazy(lua_State* L, int idx) {
    return static_cast<LazyUserData*>(luaL_testudata(L, idx, LazyMeta));
}

LazyUserData& CheckLazy(lua_State* L, int idx) {
    auto* ud = TestLazy(L, idx);
    if (!ud) {
        luaL_argerror(L, idx, "lazy message expected");
    }
    return *ud;
}

// A materialized tree can hold a reference back to itself (`msg.self = msg`), so
// converting one re-enters ToQVariant: bound that with a Lua error instead of
// letting the C stack overflow.
constexpr int MaxConvertDepth = 32;

int convertDepth = 0;

struct ConvertDepth {
    ConvertDepth() {
        if (convertDepth >= MaxConvertDepth) {
            Raise("lazy view conversion nests too deeply (does a message refer to itself?)");
        }
        ++convertDepth;
    }
    ~ConvertDepth() { --convertDepth; }
};

// A view keeps its place in the message in two user values: 1 = the view it was
// indexed from, 2 = the key it was indexed with (the message root has neither).
// The first one is also what keeps a message alive: a live view holds its whole
// chain up to the root, which owns the message. LuaJIT has no user values, so the
// same two live in the userdata's environment (which a fresh LuaJIT userdata
// would otherwise inherit from the globals). The root's third user value holds
// the message's table once it is materialized: a user value, not a registry
// reference, so a view stored inside the message (`msg.copy = msg.a`) is one
// collectable cycle instead of a table pinned until the process ends.
void Connect(lua_State* L, int keepIdx, int keyIdx) {
    int viewIdx = lua_gettop(L);
#ifdef RADAPTER_JIT
    lua_newtable(L);
    if (keepIdx) {
        lua_pushvalue(L, keepIdx);
        lua_rawseti(L, -2, 1);
    }
    if (keyIdx) {
        lua_pushvalue(L, keyIdx);
        lua_rawseti(L, -2, 2);
    }
    lua_setfenv(L, viewIdx);
#else
    if (keepIdx) {
        lua_pushvalue(L, keepIdx);
        lua_setiuservalue(L, viewIdx, 1);
    }
    if (keyIdx) {
        lua_pushvalue(L, keyIdx);
        lua_setiuservalue(L, viewIdx, 2);
    }
#endif
}

void PushLink(lua_State* L, int idx, int slot) {
    idx = compat::lua_absindex(L, idx);
#ifdef RADAPTER_JIT
    lua_getfenv(L, idx);
    lua_rawgeti(L, -1, slot);
    lua_remove(L, -2);
#else
    lua_getiuservalue(L, idx, slot);
#endif
}

// Creates the view userdata for `value`, a child of the view at `keepIdx` under the
// key at `keyIdx`.
void PushView(lua_State* L, LazyOwner* owner, LazyUserData* parent, int keepIdx, int keyIdx,
              QVariant const& value) {
    keepIdx = compat::lua_absindex(L, keepIdx);
    keyIdx = compat::lua_absindex(L, keyIdx);
#ifdef RADAPTER_JIT
    auto* ud = static_cast<LazyUserData*>(lua_newuserdata(L, sizeof(LazyUserData)));
#else
    auto* ud = static_cast<LazyUserData*>(lua_newuserdatauv(L, sizeof(LazyUserData), 2));
#endif
    new (ud) LazyUserData{owner, parent, value};
    Connect(L, keepIdx, keyIdx);
    luaL_getmetatable(L, LazyMeta);
    lua_setmetatable(L, -2);
}

void PushRoot(lua_State* L, QVariant const& v) {
#ifdef RADAPTER_JIT
    auto* mem = lua_newuserdata(L, RootSize);
#else
    auto* mem = lua_newuserdatauv(L, RootSize, 3);
#endif
    auto* owner = new (static_cast<char*>(mem) + sizeof(LazyUserData)) LazyOwner{v, false};
    new (static_cast<LazyUserData*>(mem)) LazyUserData{owner, nullptr, v};
    Connect(L, 0, 0);
    luaL_getmetatable(L, LazyMeta);
    lua_setmetatable(L, -2);
}

void PushRootView(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    lua_pushvalue(L, idx);
    int cur = lua_gettop(L);
    while (true) {
        PushLink(L, cur, 1);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return;
        }
        lua_replace(L, cur);
    }
}

void PushMessageTable(lua_State* L, int idx) {
    PushLink(L, idx, 3);
}

// Consumes the value on top.
void SetMessageTable(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
#ifdef RADAPTER_JIT
    lua_getfenv(L, idx);
    lua_insert(L, -2);
    lua_rawseti(L, -2, 3);
    lua_pop(L, 1);
#else
    lua_setiuservalue(L, idx, 3);
#endif
}

// Turns the retained tree into the plain Lua table every view of the message
// resolves through from now on.
void MaterializeMessage(lua_State* L, int idx, LazyOwner& owner) {
    if (owner.materialized) {
        return;
    }
    idx = compat::lua_absindex(L, idx);
    PushRootView(L, idx);
    int rootIdx = lua_gettop(L);
    glua::Push(L, owner.retained);
    SetMessageTable(L, rootIdx);
    owner.materialized = true;
    lua_pop(L, 1);
}

// Pushes the keys leading from the message root to the view at `idx` (the view's
// own key first, the key just below the root last) and then the message's table.
// Returns the key count.
int PushPath(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    lua_pushvalue(L, idx);
    int cur = lua_gettop(L);
    int depth = 0;
    while (true) {
        PushLink(L, cur, 1);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            break;
        }
        PushLink(L, cur, 2);
        lua_insert(L, -2);
        lua_replace(L, cur);
        ++depth;
    }
    PushMessageTable(L, cur);
    lua_remove(L, cur);
    return depth;
}

// Pushes what the view at `idx` is in its message's materialized table. Indexing
// the result is indexing the message, so a missing level raises like Lua does.
void PushResolved(lua_State* L, int idx, LazyUserData const& ud) {
    idx = compat::lua_absindex(L, idx);
    if (!ud.parent) {
        PushMessageTable(L, idx);
        return;
    }
    int depth = PushPath(L, idx);
    int table = lua_gettop(L);
    for (int i = table - 1; i >= table - depth; --i) {
        lua_pushvalue(L, i);
        lua_gettable(L, -2);
        lua_remove(L, -2);
    }
    int first = table - depth;
    lua_replace(L, first);
    lua_settop(L, first);
}

void PushChildView(lua_State* L, int viewIdx, LazyUserData& parent, QVariant const& value,
                   int keyIdx) {
    viewIdx = compat::lua_absindex(L, viewIdx);
    keyIdx = compat::lua_absindex(L, keyIdx);
    auto type = value.metaType().id();
    if (type != QMetaType::QVariantMap && type != QMetaType::QVariantList) {
        glua::Push(L, value);
        return;
    }
    PushView(L, parent.owner, &parent, viewIdx, keyIdx, value);
}

void PushIndexed(lua_State* L, int viewIdx, LazyUserData& ud, int keyIdx) {
    viewIdx = compat::lua_absindex(L, viewIdx);
    keyIdx = compat::lua_absindex(L, keyIdx);
    if (ud.owner->materialized) {
        PushResolved(L, viewIdx, ud);
        lua_pushvalue(L, keyIdx);
        lua_gettable(L, -2);
        lua_remove(L, -2);
        return;
    }
    auto type = ud.value.metaType().id();
    if (lua_type(L, keyIdx) == LUA_TSTRING) {
        if (type == QMetaType::QVariantMap) {
            size_t len = 0;
            auto* s = lua_tolstring(L, keyIdx, &len);
            auto const* map = static_cast<QVariantMap const*>(ud.value.constData());
            auto it = map->constFind(QString::fromUtf8(s, int(len)));
            if (it != map->cend()) {
                PushChildView(L, viewIdx, ud, it.value(), keyIdx);
                return;
            }
        }
        lua_pushnil(L);
        return;
    }
    if (lua_type(L, keyIdx) == LUA_TNUMBER && type == QMetaType::QVariantList) {
        auto const* list = static_cast<QVariantList const*>(ud.value.constData());
        auto idx = lua_tointeger(L, keyIdx);
        if (idx >= 1 && idx <= list->size()) {
            PushChildView(L, viewIdx, ud, (*list)[int(idx) - 1], keyIdx);
            return;
        }
    }
    lua_pushnil(L);
}

lua_Integer ListLength(lua_State* L, int idx) {
#ifdef RADAPTER_JIT
    return lua_Integer(lua_objlen(L, idx));
#else
    return luaL_len(L, idx);
#endif
}

int NodeGc(lua_State* L) {
    auto* ud = static_cast<LazyUserData*>(lua_touserdata(L, 1));
    if (!ud->parent) {
        ud->owner->~LazyOwner();
    }
    ud->~LazyUserData();
    return 0;
}

int NodeMaterialize(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    MaterializeMessage(L, 1, *ud.owner);
    PushResolved(L, 1, ud);
    return 1;
}

int NodeToString(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    QVariant value = ud.value;
    if (ud.owner->materialized) {
        PushResolved(L, 1, ud);
        value = builtin::help::toQVar(L, -1);
        lua_pop(L, 1);
    }
    auto json = QJsonDocument::fromVariant(builtin::help::reprBinaries(std::move(value)))
                    .toJson(QJsonDocument::Compact);
    lua_pushlstring(L, json.constData(), size_t(json.size()));
    return 1;
}

int NodeLen(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    if (ud.owner->materialized) {
        PushResolved(L, 1, ud);
        lua_pushinteger(L, ListLength(L, -1));
        return 1;
    }
    if (ud.value.metaType().id() == QMetaType::QVariantList) {
        auto const* list = static_cast<QVariantList const*>(ud.value.constData());
        lua_pushinteger(L, list->size());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int NodeIndex(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        size_t len = 0;
        auto* s = lua_tolstring(L, 2, &len);
        if (std::string_view(s, len) == "materialize"
            && luaL_getmetafield(L, 1, "materialize")) {
            return 1;
        }
    }
    PushIndexed(L, 1, ud, 2);
    return 1;
}

int NodeNewIndex(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    MaterializeMessage(L, 1, *ud.owner);
    PushResolved(L, 1, ud);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_settable(L, -3);
    return 0;
}

// next()-style: the control value is the previous key, so no position list is
// built and a step stays O(1).
int NextPair(lua_State* L) {
    int viewIdx = lua_upvalueindex(1);
    auto& view = CheckLazy(L, viewIdx);
    if (view.owner->materialized) {
        PushResolved(L, viewIdx, view);
        lua_getglobal(L, "next");
        lua_insert(L, -2);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 2);
        return 2;
    }
    if (view.value.metaType().id() == QMetaType::QVariantList) {
        auto const* list = static_cast<QVariantList const*>(view.value.constData());
        lua_Integer i = lua_isnoneornil(L, 2) ? 1 : lua_tointeger(L, 2) + 1;
        if (i < 1 || i > list->size()) {
            return 0;
        }
        lua_pushinteger(L, i);
        PushChildView(L, viewIdx, view, (*list)[int(i) - 1], lua_gettop(L));
        return 2;
    }
    auto const* map = static_cast<QVariantMap const*>(view.value.constData());
    QVariantMap::const_iterator it;
    if (lua_isnoneornil(L, 2)) {
        it = map->cbegin();
    } else {
        size_t len = 0;
        auto* key = lua_tolstring(L, 2, &len);
        it = map->upperBound(QString::fromUtf8(key, int(len)));
    }
    if (it == map->cend()) {
        return 0;
    }
    auto key = it.key().toUtf8();
    lua_pushlstring(L, key.constData(), size_t(key.size()));
    PushChildView(L, viewIdx, view, it.value(), lua_gettop(L));
    return 2;
}

int NextIndex(lua_State* L) {
    int viewIdx = lua_upvalueindex(1);
    auto& view = CheckLazy(L, viewIdx);
    if (view.owner->materialized) {
        PushResolved(L, viewIdx, view);
        auto i = lua_tointeger(L, 2) + 1;
        lua_pushinteger(L, i);
        lua_gettable(L, -2);
        lua_remove(L, -2);
        if (lua_isnil(L, -1)) {
            return 0;
        }
        lua_pushinteger(L, i);
        lua_insert(L, -2);
        return 2;
    }
    if (view.value.metaType().id() != QMetaType::QVariantList) {
        return 0;
    }
    auto const* list = static_cast<QVariantList const*>(view.value.constData());
    auto i = lua_tointeger(L, 2) + 1;
    if (i < 1 || i > list->size()) {
        return 0;
    }
    lua_pushinteger(L, i);
    PushChildView(L, viewIdx, view, (*list)[int(i) - 1], lua_gettop(L));
    return 2;
}

int NodePairs(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    if (ud.owner->materialized) {
        PushResolved(L, 1, ud);
        lua_getglobal(L, "next");
        lua_insert(L, -2);
        lua_pushnil(L);
        return 3;
    }
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, builtin::protect<NextPair>, 1);
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
}

int NodeIpairs(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    if (ud.owner->materialized) {
        PushResolved(L, 1, ud);
        lua_getglobal(L, "ipairs");
        lua_insert(L, -2);
        lua_call(L, 1, 3);
        return 3;
    }
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, builtin::protect<NextIndex>, 1);
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 0);
    return 3;
}

#ifdef RADAPTER_JIT
int delegate(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

// Lua 5.1 has no __pairs/__ipairs, so the globals are wrapped: a lazy view gets
// the engine iterator, everything else reaches the original.
int Pairs(lua_State* L) {
    if (TestLazy(L, 1)) {
        return NodePairs(L);
    }
    return delegate(L);
}

int Ipairs(lua_State* L) {
    if (TestLazy(L, 1)) {
        return NodeIpairs(L);
    }
    return delegate(L);
}

void installWrapper(lua_State* L, const char* name, lua_CFunction wrapper) {
    lua_getglobal(L, name);
    lua_pushcclosure(L, wrapper, 1);
    lua_setglobal(L, name);
}
#endif

}

namespace radapter::lazy {

void Init(lua_State* L) {
    luaL_newmetatable(L, LazyMeta);
    lua_pushliteral(L, "LazyNode");
    lua_setfield(L, -2, "__name");
    lua_pushcfunction(L, builtin::protect<NodeGc>);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, builtin::protect<NodeIndex>);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, builtin::protect<NodeNewIndex>);
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, builtin::protect<NodeLen>);
    lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, builtin::protect<NodeToString>);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, builtin::protect<NodePairs>);
    lua_setfield(L, -2, "__pairs");
    lua_pushcfunction(L, builtin::protect<NodeIpairs>);
    lua_setfield(L, -2, "__ipairs");
    lua_pushcfunction(L, builtin::protect<NodeMaterialize>);
    lua_setfield(L, -2, "materialize");
    lua_pop(L, 1);

#ifdef RADAPTER_JIT
    installWrapper(L, "pairs", builtin::protect<Pairs>);
    installWrapper(L, "ipairs", builtin::protect<Ipairs>);
#endif
}

bool IsLazy(lua_State* L, int idx) {
    return TestLazy(L, idx) != nullptr;
}

bool ToQVariant(lua_State* L, int idx, QVariant& out) {
    auto* ud = TestLazy(L, idx);
    if (!ud) {
        return false;
    }
    ConvertDepth depth;
    idx = compat::lua_absindex(L, idx);
    if (ud->owner->materialized) {
        PushResolved(L, idx, *ud);
        out = builtin::help::toQVar(L, -1);
        lua_pop(L, 1);
    } else {
        out = ud->value;
    }
    return true;
}

void Push(lua_State* L, QVariant const& v) {
    auto type = v.metaType().id();
    if (type != QMetaType::QVariantMap && type != QMetaType::QVariantList) {
        glua::Push(L, v);
        return;
    }
    PushRoot(L, v);
}

void MaterializeInPlace(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    auto& ud = CheckLazy(L, idx);
    MaterializeMessage(L, idx, *ud.owner);
    PushResolved(L, idx, ud);
    lua_replace(L, idx);
}

int GetPath(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    auto path = builtin::help::toSV(L, 2);
    auto sep = builtin::help::Sep(L, 3, 3, "get");
    if (ud.owner->materialized) {
        PushResolved(L, 1, ud);
        lua_replace(L, 1);
        return builtin::api::Get(L);
    }
    if (path.empty() || path == sep) {
        lua_pushvalue(L, 1);
        return 1;
    }
    lua_pushvalue(L, 1);
    int cur = lua_gettop(L);
    size_t pos = 0;
    while (true) {
        auto end = path.find(sep, pos);
        bool last = end == string_view::npos;
        auto part = path.substr(pos, last ? string_view::npos : end - pos);
        builtin::help::PushPathPart(L, part);
        auto& cur_view = CheckLazy(L, cur);
        PushIndexed(L, cur, cur_view, lua_gettop(L));
        lua_replace(L, cur + 1);
        lua_replace(L, cur);
        if (last) {
            return 1;
        }
        if (!TestLazy(L, cur)) {
            lua_pushnil(L);
            return 1;
        }
        pos = end + sep.size();
    }
}

}
