// Benchmark-only plugin: a `Bench` worker that exposes the Lua <-> QVariant message
// boundary with no side effects, plus a lazy message view used to prototype the
// lazy-indexing proposal in bench/LAZY_INDEXING.md.
//
// Load with load_plugin("radapter_bench_plugin") from bench/*.lua.

#include "radapter/radapter.hpp"
#include "radapter/value.hpp"
#include "radapter/worker.hpp"
#include <QString>
#include <QVariant>
#include <lua.hpp>
#include <string_view>

using namespace radapter;

namespace {

struct BenchConfig : WorkerConfig {};
RAD_DESCRIBE(BenchConfig) {
    PARENT(WorkerConfig);
}

// --- minimal QVariant -> Lua push (the SDK does not export glua::Push(QVariant)) ---

void pushQString(lua_State* L, QString const& s) {
    auto utf8 = s.toUtf8();
    lua_pushlstring(L, utf8.constData(), size_t(utf8.size()));
}

void pushPlain(lua_State* L, QVariant const& v) {
    switch (v.metaType().id()) {
    case QMetaType::QVariantMap: {
        auto const* m = static_cast<QVariantMap const*>(v.constData());
        lua_createtable(L, 0, int(m->size()));
        for (auto it = m->cbegin(); it != m->cend(); ++it) {
            pushQString(L, it.key());
            pushPlain(L, it.value());
            lua_settable(L, -3);
        }
        break;
    }
    case QMetaType::QVariantList: {
        auto const* l = static_cast<QVariantList const*>(v.constData());
        lua_createtable(L, int(l->size()), 0);
        for (int i = 0; i < l->size(); ++i) {
            pushPlain(L, (*l)[i]);
            lua_rawseti(L, -2, i + 1);
        }
        break;
    }
    case QMetaType::QStringList: {
        auto list = v.toStringList();
        lua_createtable(L, int(list.size()), 0);
        for (int i = 0; i < list.size(); ++i) {
            pushQString(L, list[i]);
            lua_rawseti(L, -2, i + 1);
        }
        break;
    }
    case QMetaType::Bool:
        lua_pushboolean(L, v.toBool());
        break;
    case QMetaType::SChar:
    case QMetaType::Short:
    case QMetaType::Int:
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        lua_pushinteger(L, v.toLongLong());
        break;
    case QMetaType::Float:
    case QMetaType::Double:
        lua_pushnumber(L, v.toDouble());
        break;
    case QMetaType::QString:
        pushQString(L, v.toString());
        break;
    case QMetaType::QByteArray: {
        auto b = v.toByteArray();
        lua_pushlstring(L, b.constData(), size_t(b.size()));
        break;
    }
    default:
        if (v.value<LuaValue>()) {
            v.value<LuaValue>().Push(L);
        } else {
            lua_pushnil(L);
        }
        break;
    }
}

// --- lazy message view -------------------------------------------------------
//
// A userdata holding the still-converted QVariant subtree. `__index` converts
// only the requested child: scalars are pushed directly, containers become a
// nested lazy view, so reading one deep field never materializes siblings.

constexpr const char* LazyMeta = "radapter.bench.lazy";
constexpr const char* LazyCacheMeta = "radapter.bench.lazy.cache";

struct LazyNode {
    QVariant value;
};

void pushLazy(lua_State* L, QVariant v, bool cache);

int lazyIndex(lua_State* L);
int lazyIndexCached(lua_State* L);

LazyNode* checkLazy(lua_State* L, int idx) {
    if (auto* node = static_cast<LazyNode*>(luaL_testudata(L, idx, LazyMeta))) {
        return node;
    }
    return static_cast<LazyNode*>(luaL_checkudata(L, idx, LazyCacheMeta));
}

int lazyMaterialize(lua_State* L) {
    auto* node = checkLazy(L, 1);
    pushPlain(L, node->value);
    return 1;
}

int lazyType(lua_State* L) {
    auto* node = checkLazy(L, 1);
    lua_pushstring(L, node->value.metaType().name());
    return 1;
}

int lazyGc(lua_State* L) {
    static_cast<LazyNode*>(lua_touserdata(L, 1))->~LazyNode();
    return 0;
}

// Pushes the node's child cache table, creating it on first access. Lua 5.1
// (LuaJIT) has no user values, so the cache lives in the userdata environment.
void pushCache(lua_State* L, int idx) {
#ifdef RADAPTER_JIT
    lua_getfenv(L, idx);
    if (!lua_isnil(L, -1)) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfenv(L, idx);
#else
    lua_getiuservalue(L, idx, 1);
    if (!lua_isnil(L, -1)) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setiuservalue(L, idx, 1);
#endif
}

// Allocates the userdata directly: a real implementation would do the same,
// whereas LuaUserData::Create goes through an extra registry ref per node.
void pushLazy(lua_State* L, QVariant v, bool cache) {
    const char* meta = cache ? LazyCacheMeta : LazyMeta;
#ifdef RADAPTER_JIT
    auto* mem = lua_newuserdata(L, sizeof(LazyNode));
    if (cache) {
        lua_newtable(L);
        lua_setfenv(L, -2);
    }
#else
    auto* mem = lua_newuserdatauv(L, sizeof(LazyNode), cache ? 1 : 0);
    if (cache) {
        lua_newtable(L);
        lua_setiuservalue(L, -2, 1);
    }
#endif
    new (mem) LazyNode{std::move(v)};
    luaL_getmetatable(L, meta);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_newmetatable(L, meta);
        lua_pushcfunction(L, lazyGc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, cache ? lazyIndexCached : lazyIndex);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

void pushChild(lua_State* L, QVariant const& v, bool cache) {
    auto t = v.metaType().id();
    if (t == QMetaType::QVariantMap || t == QMetaType::QVariantList) {
        pushLazy(L, v, cache);
    } else {
        pushPlain(L, v);
    }
}

int pushIndexed(lua_State* L, LazyNode* node, bool cache) {
    if (lua_type(L, 2) == LUA_TSTRING) {
        size_t len = 0;
        auto* s = lua_tolstring(L, 2, &len);
        std::string_view key(s, len);
        if (key == "materialize") {
            lua_pushcfunction(L, lazyMaterialize);
            return 1;
        }
        if (key == "type_name") {
            lua_pushcfunction(L, lazyType);
            return 1;
        }
        if (node->value.metaType().id() == QMetaType::QVariantMap) {
            auto const* m = static_cast<QVariantMap const*>(node->value.constData());
            auto it = m->constFind(QString::fromUtf8(s, int(len)));
            if (it == m->cend()) {
                lua_pushnil(L);
            } else {
                pushChild(L, it.value(), cache);
            }
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    if (lua_type(L, 2) == LUA_TNUMBER) {
        if (node->value.metaType().id() == QMetaType::QVariantList) {
            auto const* list = static_cast<QVariantList const*>(node->value.constData());
            auto idx = lua_tointeger(L, 2);
            if (idx >= 1 && idx <= list->size()) {
                pushChild(L, (*list)[int(idx) - 1], cache);
            } else {
                lua_pushnil(L);
            }
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int lazyIndex(lua_State* L) {
    auto* node = checkLazy(L, 1);
    return pushIndexed(L, node, false);
}

int lazyIndexCached(lua_State* L) {
    auto* node = checkLazy(L, 1);
    pushCache(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, -2);
        return 1;
    }
    lua_pop(L, 1);
    pushIndexed(L, node, true);
    lua_pushvalue(L, 2);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
    lua_remove(L, -2);
    return 1;
}

// --- message tree builder ----------------------------------------------------

QVariant buildDeep(int depth, int width) {
    if (depth <= 0) {
        QVariantList list;
        list.reserve(width);
        for (int i = 0; i < width; ++i) list.append(i);
        return list;
    }
    QVariantMap m;
    m.insert(QStringLiteral("a"), buildDeep(depth - 1, width));
    for (int i = 1; i < width; ++i) {
        m.insert(QStringLiteral("k") + QString::number(i), i);
    }
    return m;
}

int argInt(QVariantList const& args, int idx, int def) {
    if (args.size() <= idx) return def;
    return int(args[idx].toLongLong());
}

class BenchWorker final : public Worker {
    Q_OBJECT
    QVariant cached;
public:
    BenchWorker(BenchConfig conf, Instance* inst) :
        Worker(inst, conf, "bench")
    {}

    void OnMsg(QVariant const&) override {}

    QVariant Sink(QVariantList const&) { return {}; }

    QVariant Noop(QVariantList const&) { return {}; }

    QVariant Roundtrip(QVariantList const& args) { return args.value(0); }

    QVariant Echo(QVariantList const& args) {
        if (!args.isEmpty()) {
            emit SendMsg(args.first());
        }
        return {};
    }

    QVariant Deep(QVariantList const& args) {
        return buildDeep(argInt(args, 0, 1), argInt(args, 1, 1));
    }

    QVariant Prepare(QVariantList const& args) {
        cached = buildDeep(argInt(args, 0, 1), argInt(args, 1, 1));
        return {};
    }

    QVariant DeepCached(QVariantList const&) { return cached; }

    QVariant Lazy(QVariantList const& args) {
        auto* L = _Inst->LuaState();
        pushLazy(L, buildDeep(argInt(args, 0, 1), argInt(args, 1, 1)), false);
        LuaValue ud(L, ConsumeTop);
        return QVariant::fromValue<LuaValue>(ud);
    }

    QVariant LazyView(QVariantList const&) {
        auto* L = _Inst->LuaState();
        pushLazy(L, cached, false);
        LuaValue ud(L, ConsumeTop);
        return QVariant::fromValue<LuaValue>(ud);
    }

    QVariant LazyViewCached(QVariantList const&) {
        auto* L = _Inst->LuaState();
        pushLazy(L, cached, true);
        LuaValue ud(L, ConsumeTop);
        return QVariant::fromValue<LuaValue>(ud);
    }
};

} // namespace

RADAPTER_PLUGIN(Bench, "radapter.bench.plugin") {
    radapter->RegisterWorker<BenchWorker>("Bench", {
        {"Sink", AsExtraMethod<&BenchWorker::Sink>},
        {"Noop", AsExtraMethod<&BenchWorker::Noop>},
        {"Roundtrip", AsExtraMethod<&BenchWorker::Roundtrip>},
        {"Echo", AsExtraMethod<&BenchWorker::Echo>},
        {"Deep", AsExtraMethod<&BenchWorker::Deep>},
        {"Prepare", AsExtraMethod<&BenchWorker::Prepare>},
        {"DeepCached", AsExtraMethod<&BenchWorker::DeepCached>},
        {"Lazy", AsExtraMethod<&BenchWorker::Lazy>},
        {"LazyView", AsExtraMethod<&BenchWorker::LazyView>},
        {"LazyViewCached", AsExtraMethod<&BenchWorker::LazyViewCached>},
    });
    radapter->RegisterSchema("Bench", SchemaFor<BenchConfig>);
}

#include "bench_plugin.moc"
