#include "lazy.hpp"
#include "builtin.hpp"
#include "utils.hpp"
#include "glua/glua.hpp"
#include <QJsonDocument>

using namespace radapter;

namespace {

constexpr const char* LazyMeta = "radapter.LazyNode";

struct LazyUserData {
    LazyNode node;
    bool root; // a message handed to a listener; derived views are not writable
};

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

// A derived view (children, get() results) is a temporary over the sender's
// tree: an assignment through it cannot reach the message, so it is an error
// rather than a silently dropped write -- even once it was materialized.
void RequireWritable(LazyUserData const& ud) {
    if (!ud.root) {
        Raise("nested lazy views are read-only: materialize() one and edit the returned "
              "table, or set() the field on the message");
    }
}

// A materialized view can hold a reference back to itself (`msg.self = msg`), so
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

// A view that was written to keeps a plain table in its user value (environment
// on Lua 5.1): the retained tree is only the immutable source, copy-on-write.
constexpr const char* MaterializedKey = "__materialized";

void PushUserValue(lua_State* L, int idx) {
#ifdef RADAPTER_JIT
    lua_getfenv(L, idx);
#else
    lua_getiuservalue(L, idx, 1);
#endif
}

void SetUserValue(lua_State* L, int idx) {
#ifdef RADAPTER_JIT
    lua_setfenv(L, idx);
#else
    lua_setiuservalue(L, idx, 1);
#endif
}

// A fresh LuaJIT userdata inherits the globals as its environment, so PushNode
// installs an empty table there instead.
void PushEnvTable(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    PushUserValue(L, idx);
    if (lua_istable(L, -1)) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    SetUserValue(L, idx);
}

bool PushMaterialized(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    PushUserValue(L, idx);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    lua_getfield(L, -1, MaterializedKey);
    lua_remove(L, -2);
    if (lua_istable(L, -1)) {
        return true;
    }
    lua_pop(L, 1);
    return false;
}

void StoreMaterialized(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    lua_pushvalue(L, -1);
    PushEnvTable(L, idx);
    lua_insert(L, -2);
    lua_setfield(L, -2, MaterializedKey);
    lua_pop(L, 1);
}

void PushMaterializedOrCreate(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    if (!PushMaterialized(L, idx)) {
        glua::Push(L, CheckLazy(L, idx).node.value);
        StoreMaterialized(L, idx);
    }
}

void PushNode(lua_State* L, QVariant const& v, bool root) {
#ifdef RADAPTER_JIT
    auto* ud = static_cast<LazyUserData*>(lua_newuserdata(L, sizeof(LazyUserData)));
#else
    auto* ud = static_cast<LazyUserData*>(lua_newuserdatauv(L, sizeof(LazyUserData), 1));
#endif
    new (ud) LazyUserData{LazyNode{v}, root};
#ifdef RADAPTER_JIT
    lua_newtable(L);
    lua_setfenv(L, -2);
#endif
    luaL_getmetatable(L, LazyMeta);
    lua_setmetatable(L, -2);
}

// A view only makes sense over a container; everything else is pushed as the
// value it is (the container-body accessors assume a map or a list).
void PushAny(lua_State* L, QVariant const& v, bool root) {
    switch (v.metaType().id()) {
    case QMetaType::QVariantMap:
    case QMetaType::QVariantList:
        PushNode(L, v, root);
        break;
    default:
        glua::Push(L, v);
        break;
    }
}

void PushChild(lua_State* L, QVariant const& v) {
    PushAny(L, v, false);
}

int NodeGc(lua_State* L) {
    static_cast<LazyUserData*>(lua_touserdata(L, 1))->~LazyUserData();
    return 0;
}

int NodeMaterialize(lua_State* L) {
    CheckLazy(L, 1);
    PushMaterializedOrCreate(L, 1);
    return 1;
}

int NodeToString(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    QVariant value = ud.node.value;
    if (PushMaterialized(L, 1)) {
        value = builtin::help::toQVar(L, -1);
        lua_pop(L, 1);
    }
    auto json = QJsonDocument::fromVariant(builtin::help::reprBinaries(std::move(value)))
                    .toJson(QJsonDocument::Compact);
    lua_pushlstring(L, json.constData(), size_t(json.size()));
    return 1;
}

int NodeLen(lua_State* L) {
    CheckLazy(L, 1);
    if (PushMaterialized(L, 1)) {
#ifdef RADAPTER_JIT
        auto len = lua_Integer(lua_objlen(L, -1));
#else
        auto len = lua_Integer(lua_rawlen(L, -1));
#endif
        lua_pop(L, 1);
        lua_pushinteger(L, len);
        return 1;
    }
    auto& ud = CheckLazy(L, 1);
    if (ud.node.value.metaType().id() == QMetaType::QVariantList) {
        auto const* list = static_cast<QVariantList const*>(ud.node.value.constData());
        lua_pushinteger(L, list->size());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

int NodeIndex(lua_State* L) {
    CheckLazy(L, 1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        size_t len = 0;
        auto* s = lua_tolstring(L, 2, &len);
        if (std::string_view(s, len) == "materialize"
            && luaL_getmetafield(L, 1, "materialize")) {
            return 1;
        }
    }
    if (PushMaterialized(L, 1)) {
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        lua_remove(L, -2);
        return 1;
    }
    auto& ud = CheckLazy(L, 1);
    auto type = ud.node.value.metaType().id();
    if (lua_type(L, 2) == LUA_TSTRING) {
        size_t len = 0;
        auto* s = lua_tolstring(L, 2, &len);
        std::string_view key(s, len);
        if (type == QMetaType::QVariantMap) {
            auto const* map = static_cast<QVariantMap const*>(ud.node.value.constData());
            auto it = map->constFind(QString::fromUtf8(s, int(len)));
            if (it == map->cend()) {
                lua_pushnil(L);
            } else {
                PushChild(L, it.value());
            }
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    if (lua_type(L, 2) == LUA_TNUMBER && type == QMetaType::QVariantList) {
        auto const* list = static_cast<QVariantList const*>(ud.node.value.constData());
        auto idx = lua_tointeger(L, 2);
        if (idx >= 1 && idx <= list->size()) {
            PushChild(L, (*list)[int(idx) - 1]);
        } else {
            lua_pushnil(L);
        }
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int NodeNewIndex(lua_State* L) {
    auto& ud = CheckLazy(L, 1);
    RequireWritable(ud);
    if (!PushMaterialized(L, 1)) {
        glua::Push(L, ud.node.value);
        StoreMaterialized(L, 1);
    }
    lua_insert(L, 2);
    lua_settable(L, 2);
    return 0;
}

// next()-style iterator: the control value is the previous key, so `pairs`
// walks the retained tree without materializing it, yielding scalars eagerly
// and children as views.
int NextPair(lua_State* L) {
    auto& ud = CheckLazy(L, lua_upvalueindex(1));
    bool first = lua_isnoneornil(L, 2);
    if (ud.node.value.metaType().id() == QMetaType::QVariantList) {
        auto const* list = static_cast<QVariantList const*>(ud.node.value.constData());
        lua_Integer i = first ? 1 : lua_tointeger(L, 2) + 1;
        if (i < 1 || i > list->size()) {
            return 0;
        }
        lua_pushinteger(L, i);
        PushChild(L, (*list)[int(i) - 1]);
        return 2;
    }
    auto const* map = static_cast<QVariantMap const*>(ud.node.value.constData());
    QVariantMap::const_iterator it;
    if (first) {
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
    PushChild(L, it.value());
    return 2;
}

int NextIndex(lua_State* L) {
    auto& ud = CheckLazy(L, lua_upvalueindex(1));
    if (ud.node.value.metaType().id() != QMetaType::QVariantList) {
        return 0;
    }
    auto const* list = static_cast<QVariantList const*>(ud.node.value.constData());
    auto i = lua_tointeger(L, 2) + 1;
    if (i < 1 || i > list->size()) {
        return 0;
    }
    lua_pushinteger(L, i);
    PushChild(L, (*list)[int(i) - 1]);
    return 2;
}

int NodePairs(lua_State* L) {
    CheckLazy(L, 1);
    if (PushMaterialized(L, 1)) {
        lua_getglobal(L, "next");
        lua_pushvalue(L, -2);
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
    CheckLazy(L, 1);
    if (PushMaterialized(L, 1)) {
        lua_getglobal(L, "ipairs");
        lua_pushvalue(L, -2);
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
    if (PushMaterialized(L, idx)) {
        out = builtin::help::toQVar(L, -1);
        lua_pop(L, 1);
    } else {
        out = ud->node.value;
    }
    return true;
}

void Push(lua_State* L, QVariant const& v) {
    PushAny(L, v, true);
}

void PushChild(lua_State* L, QVariant const& v) {
    ::PushChild(L, v);
}

void MaterializeInPlace(lua_State* L, int idx) {
    idx = compat::lua_absindex(L, idx);
    auto& ud = CheckLazy(L, idx);
    RequireWritable(ud);
    if (PushMaterialized(L, idx)) {
        lua_replace(L, idx);
        return;
    }
    glua::Push(L, ud.node.value);
    StoreMaterialized(L, idx);
    lua_replace(L, idx);
}

int GetPath(lua_State* L) {
    if (PushMaterialized(L, 1)) {
        lua_replace(L, 1);
        return builtin::api::Get(L);
    }
    auto& ud = CheckLazy(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    auto path = builtin::help::toSV(L, 2);
    auto sep = builtin::help::Sep(L, 3, 3, "get");
    if (path.empty() || path == sep) {
        PushChild(L, ud.node.value);
        return 1;
    }
    auto cur = ud.node.value;
    size_t pos = 0;
    while (true) {
        auto end = path.find(sep, pos);
        bool last = end == string_view::npos;
        auto part = path.substr(pos, last ? string_view::npos : end - pos);
        switch (cur.metaType().id()) {
        case QMetaType::QVariantMap: {
            auto const* map = static_cast<QVariantMap const*>(cur.constData());
            auto it = map->constFind(QString::fromUtf8(part.data(), int(part.size())));
            if (it == map->cend()) {
                lua_pushnil(L);
                return 1;
            }
            cur = it.value();
            break;
        }
        case QMetaType::QVariantList: {
            auto n = tryInt(part);
            auto const* list = static_cast<QVariantList const*>(cur.constData());
            if (n < 1 || n > list->size()) {
                lua_pushnil(L);
                return 1;
            }
            cur = (*list)[int(n) - 1];
            break;
        }
        default:
            lua_pushnil(L);
            return 1;
        }
        if (last) {
            break;
        }
        pos = end + sep.size();
    }
    PushChild(L, cur);
    return 1;
}

}
