#include <radapter/radapter.hpp>
#include <QWidget>
#include <QMainWindow>
#include <QMetaMethod>
#include <QPointer>
#include "builtin.hpp"

using CtorFunc = QObject*();

using Holder = QPointer<QObject>;

static int factoryFor(lua_State* L) {
	auto* ctor = (CtorFunc*)lua_touserdata(L, lua_upvalueindex(1));
	auto* meta = (const QMetaObject*)lua_touserdata(L, lua_upvalueindex(2));
	auto ud = lua_udata(L, sizeof(Holder));
	new (ud) Holder{ ctor() };
	luaL_setmetatable(L, meta->className());
	return 1;
}

static int methodEntry(lua_State* L) {
	auto* meta = (const QMetaObject*)lua_touserdata(L, lua_upvalueindex(1));
	auto mid = lua_tointeger(L, lua_upvalueindex(2));
	QMetaMethod method = meta->method(mid);
	auto sig = method.methodSignature().toStdString();
	QObject* self = static_cast<Holder*>(luaL_checkudata(L, 1, meta->className()))->data();
	if (!self) {
		luaL_error(L, "Object was destroyed");
	}
	QGenericReturnArgument ret;
	QGenericArgument a0;
	QGenericArgument a1;
	QGenericArgument a2;
	QGenericArgument a3;
	QGenericArgument a4;
	QGenericArgument a5;
	QGenericArgument a6;
	QGenericArgument a7;
	QGenericArgument a8;
	QGenericArgument a9;
	method.invoke(self, Qt::AutoConnection, ret, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
	return 0;
}

static void populateMeta(lua_State* L, const QMetaObject& meta) {
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushcclosure(L, glua::dtor_for<Holder>, 0);
	lua_setfield(L, -2, "__gc");
	auto ms = meta.methodCount();
	for (auto i = 0; i < ms; ++i) {
		auto method = meta.method(i);
		auto t = method.methodType();
		if (t == QMetaMethod::Method || t == QMetaMethod::Slot) {
			auto name = method.name().toStdString();
			lua_pushlightuserdata(L, (void*)&meta);
			lua_pushinteger(L, i);
			lua_pushcclosure(L, glua::protect<methodEntry>, 2);
			lua_setfield(L, -2, name.c_str());
		}
	}
}

template<typename T>
QObject* ctorFor() {
	return new T;
}

template<typename T>
static void registerClass(lua_State* L) {
	if (luaL_newmetatable(L, T::staticMetaObject.className())) {
		populateMeta(L, T::staticMetaObject);
		lua_pop(L, 1);
	}
	lua_pushlightuserdata(L, (void*)&ctorFor<T>);
	lua_pushlightuserdata(L, (void*)&T::staticMetaObject);
	lua_pushcclosure(L, glua::protect<factoryFor>, 2);
	lua_setfield(L, -2, T::staticMetaObject.className());
}

namespace radapter::builtin {


void gui(Instance* inst) 
{
	auto L = inst->LuaState();
	lua_gc(L, LUA_GCSTOP, 0);
	defer restart([&]{
		lua_gc(L, LUA_GCRESTART, 0);
	});
	lua_createtable(L, 0, 0);
	registerClass<QMainWindow>(L);
	lua_setglobal(L, "gui");
}

}