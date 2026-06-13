#include "tags.hpp"
#include "builtin.hpp"
#include "glua/glua.hpp"
#include "instance_impl.hpp"

namespace radapter {

static int changed_get_listeners(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    return 1;
}

static int changed_index(lua_State* L);

// build a pipable { get_listeners = () -> listeners } on top of the stack
static void pushPipable(lua_State* L, LuaValue& listeners) {
    lua_newtable(L);
    listeners.Push(L);
    lua_pushcclosure(L, changed_get_listeners, 1);
    lua_setfield(L, -2, "get_listeners");
}

static void callListeners(Instance* inst, LuaValue& listeners, QVariant const& ev) {
    if (!listeners) return;
    auto* L = inst->LuaState();
    lua_pushcfunction(L, builtin::traceback);
    auto msgh = lua_gettop(L);
    lua_getglobal(L, "call_all");
    listeners.Push(L);
    glua::Push(L, ev);
    lua_pushnil(L);
    if (lua_pcall(L, 3, 0, msgh) != LUA_OK) {
        inst->Error("tags", "call_all error: {}", lua_tostring(L, -1));
    }
    lua_settop(L, msgh - 1);
}

TagRegistry::TagRegistry(Instance* inst) : QObject(inst), _inst(inst) {
    auto* L = inst->LuaState();

    lua_newtable(L);
    changedListeners = LuaValue(L, ConsumeTop);

    pushPipable(L, changedListeners);
    changedObj = LuaValue(L, ConsumeTop);

    // metatable so `tags.changed["tag-name"]` yields a per-tag pipe target
    changedObj.Push(L);
    lua_newtable(L);
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, glua::protect<changed_index>, 1);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);

    connect(inst, &Instance::WorkerCreated, this, &TagRegistry::onWorkerCreated);
}

void TagRegistry::onWorkerCreated(Worker* w) {
    auto wname = w->objectName();
    for (auto it = _tags.begin(); it != _tags.end(); ++it) {
        if (!it.key().startsWith(wname + ":")) continue;
        auto& tag = it.value();
        if (!tag.source) {
            tag.source = w;
            tag.field = it.key().mid(wname.size() + 1);
        }
    }
}

void TagRegistry::Advertise(Worker* w, QStringList const& fields) {
    auto wname = w->objectName();
    for (auto const& field : fields) {
        auto tagName = wname + ":" + field;
        auto& tag = _tags[tagName];
        tag.source = w;
        tag.field = field;
        tag.quality = Quality::CommFail;
    }
}

void TagRegistry::Subscribe(QString const& tagName, LuaFunction fn) {
    _tags[tagName].subscribers.push_back(std::move(fn));
}

TagRegistry::Tag const* TagRegistry::GetTag(QString const& tagName) const {
    auto it = _tags.find(tagName);
    return it != _tags.end() ? &it.value() : nullptr;
}

void TagRegistry::onWorkerMsg(Worker* w, QVariant const& msg) {
    FlatMap flat;
    Flatten(flat, msg);
    auto wname = w->objectName();
    for (auto& [k, v] : flat) {
        if (!v.isValid()) continue;
        updateTag(wname + ":" + QString::fromStdString(k), v, w);
    }
}

void TagRegistry::onWorkerEvent(Worker* w, QVariant const& msg) {
    if (msg.type() != QVariant::Map) return;
    auto m = msg.toMap();

    bool disconnected = false;
    bool connected = false;

    if (m.contains("state")) {
        auto s = m["state"].toString();
        disconnected = (s == "UnconnectedState" || s == "disconnected");
        connected = (s == "ConnectedState" || s == "connected");
    }
    if (m.contains("disconnected")) disconnected = true;
    if (m.contains("connected")) connected = true;

    if (disconnected) setWorkerQuality(w, Quality::CommFail);
    else if (connected) setWorkerQuality(w, Quality::Good);
}

void TagRegistry::setWorkerQuality(Worker* w, Quality q) {
    auto wname = w->objectName();
    for (auto it = _tags.begin(); it != _tags.end(); ++it) {
        if (!it.key().startsWith(wname + ":")) continue;
        auto& tag = it.value();
        if (tag.quality == q) continue;
        tag.quality = q;
        notifyTag(it.key(), tag);
    }
}

void TagRegistry::updateTag(QString const& tagName, QVariant const& value, Worker* source) {
    auto& tag = _tags[tagName];
    if (!tag.source) {
        tag.source = source;
        tag.field = tagName.mid(source->objectName().size() + 1);
    }
    tag.value = value;
    tag.ts = QDateTime::currentMSecsSinceEpoch();
    tag.quality = Quality::Good;
    notifyTag(tagName, tag);
}

void TagRegistry::notifyTag(QString const& tagName, Tag const& tag) {
    QVariantMap ev;
    ev["name"] = tagName;
    ev["value"] = tag.value;
    ev["quality"] = QString(qualityStr(tag.quality));
    ev["ts"] = tag.ts;

    for (auto& fn : tag.subscribers) {
        try {
            fn.Call({ev});
        } catch (std::exception& e) {
            _inst->Error("tags", "subscriber error for '{}': {}", tagName, e.what());
        }
    }

    QVariant evVar(ev);
    callListeners(_inst, changedListeners, evVar);       // tags.changed
    auto it = _perTag.find(tagName);
    if (it != _perTag.end()) {
        callListeners(_inst, it.value(), evVar);         // tags.changed["name"]
    }
}

LuaValue& TagRegistry::PerTagListeners(QString const& tagName) {
    auto it = _perTag.find(tagName);
    if (it != _perTag.end()) return it.value();
    auto* L = _inst->LuaState();
    lua_newtable(L);
    return _perTag.insert(tagName, LuaValue(L, ConsumeTop)).value();
}

// Lua API functions – each captures a TagRegistry* upvalue

static TagRegistry* getRegistry(lua_State* L) {
    return static_cast<TagRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
}

// tags.changed[tagName] -> a pipe target scoped to that single tag
static int changed_index(lua_State* L) {
    auto* reg = getRegistry(L);
    auto name = QString::fromUtf8(luaL_checkstring(L, 2));
    pushPipable(L, reg->PerTagListeners(name));
    return 1;
}

static int tags_subscribe(lua_State* L) {
    auto* reg = getRegistry(L);
    // args: [1]=self (ignored), [2]=name, [3]=fn
    auto name = QString::fromUtf8(luaL_checkstring(L, 2));
    luaL_checktype(L, 3, LUA_TFUNCTION);
    reg->Subscribe(name, LuaFunction{L, 3});
    return 0;
}

static int tags_get(lua_State* L) {
    auto* reg = getRegistry(L);
    auto name = QString::fromUtf8(luaL_checkstring(L, 2));
    auto const* tag = reg->GetTag(name);
    if (!tag || !tag->value.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 3);
    glua::Push(L, tag->value);
    lua_setfield(L, -2, "value");
    lua_pushstring(L, TagRegistry::qualityStr(tag->quality));
    lua_setfield(L, -2, "quality");
    lua_pushinteger(L, tag->ts);
    lua_setfield(L, -2, "ts");
    return 1;
}

static int tags_source(lua_State* L) {
    auto* reg = getRegistry(L);
    auto name = QString::fromUtf8(luaL_checkstring(L, 2));
    auto const* tag = reg->GetTag(name);
    if (!tag || !tag->source) {
        lua_pushnil(L);
        return 1;
    }
    auto ref = tag->source->_luaSelfRef;
    if (ref == LUA_NOREF) {
        lua_pushnil(L);
        return 1;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    return 1;
}

void Instance::EnableTags() {
    if (d->tagRegistry) return;
    d->tagRegistry = std::make_unique<TagRegistry>(this);
    auto* reg = d->tagRegistry.get();
    auto* L = d->L;

    lua_newtable(L); // the `tags` global

    lua_pushlightuserdata(L, reg);
    lua_pushcclosure(L, glua::protect<tags_subscribe>, 1);
    lua_setfield(L, -2, "subscribe");

    lua_pushlightuserdata(L, reg);
    lua_pushcclosure(L, glua::protect<tags_get>, 1);
    lua_setfield(L, -2, "get");

    lua_pushlightuserdata(L, reg);
    lua_pushcclosure(L, glua::protect<tags_source>, 1);
    lua_setfield(L, -2, "source");

    reg->changedObj.Push(L);
    lua_setfield(L, -2, "changed");

    lua_setglobal(L, "tags");
}

} // namespace radapter
