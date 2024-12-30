#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include <QWidget>
#include <QMainWindow>
#include <QMetaMethod>
#include <QLabel>
#include <qlayout.h>
#include <QGridLayout>
#include <QPointer>
#include "builtin.hpp"

using namespace radapter;

using PolyCtor = QObject * (*)();
using Holder = QPointer<QObject>;

static int factoryFor(lua_State* L) {
	auto* meta = (const QMetaObject*)lua_touserdata(L, lua_upvalueindex(1));
	auto* ctor = (PolyCtor)lua_touserdata(L, lua_upvalueindex(2));
	auto ud = lua_udata(L, sizeof(Holder));
	new (ud) Holder(ctor());
	luaL_setmetatable(L, meta->className());
	return 1;
}

static int methodEntry(lua_State* L) {
	auto* meta = (const QMetaObject*)lua_touserdata(L, lua_upvalueindex(1));
	auto mid = lua_tointeger(L, lua_upvalueindex(2));
	QMetaMethod method = meta->method(mid);
	QObject* self = ((Holder*)luaL_checkudata(L, 1, meta->className()))->data();
	auto retType = method.returnType();
	QGenericReturnArgument ret;
	QVariant vret;
	if (retType != QMetaType::Void && retType != QMetaType::UnknownType) {
		vret = { QVariant::Type(retType) };
		ret = { QMetaType::typeName(retType), vret.data() };
	}
	QGenericArgument args[10];
	QVariantList passed = builtin::help::toArgs(L, 2);
	auto count = method.parameterCount();
	if (passed.size() < count) {
		throw Err("{}: Too few params: expected at least: {}", 
			method.methodSignature().toStdString(), count);
	}
	for (auto i = 0; i < count; ++i) {
		if (!passed[i].convert(method.parameterType(i))) {
			throw Err("{}: Could not convert param #{} from {} => {}", 
				method.methodSignature().toStdString(), 
				i + 1, passed[i].typeName(), QMetaType::typeName(method.parameterType(i)));
		}
		args[i] = { passed[i].typeName(), passed[i].data() };
	}
	auto status = method.invoke(self, Qt::AutoConnection, ret, 
		args[0], args[1], args[2], args[3], args[4],
		args[5], args[6], args[7], args[8], args[9]);
	if (!status) {
		throw Err("Could not invoke: {}", method.methodSignature().toStdString());
	}
	if (vret.isValid()) {
		glua::Push(L, vret);
		return 1;
	} else {
		return 0;
	}
}

//static int polyNewIndex(lua_State* L) {
//
//}
//
//static int polyIndex(lua_State* L) {
//
//}

static void populateMeta(lua_State* L, const QMetaObject* meta) {
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushcclosure(L, glua::dtor_for<Holder>, 0);
	lua_setfield(L, -2, "__gc");
	auto ms = meta->methodCount();
	for (auto i = 0; i < ms; ++i) {
		auto method = meta->method(i);
		if (method.access() != QMetaMethod::Public) {
			continue;
		}
		auto t = method.methodType();
		if (t == QMetaMethod::Method || t == QMetaMethod::Slot) {
			auto name = method.name().toStdString();
			lua_pushlightuserdata(L, (void*)meta);
			lua_pushinteger(L, i);
			lua_pushcclosure(L, glua::protect<methodEntry>, 2);
			lua_setfield(L, -2, name.c_str());
		}
	}
}

static void registerClass(lua_State* L, const QMetaObject* meta, PolyCtor ctor) {
	if (luaL_newmetatable(L, meta->className())) {
		populateMeta(L, meta);
		lua_pop(L, 1);
	}
	lua_pushlightuserdata(L, (void*)meta);
	lua_pushlightuserdata(L, (void*)ctor);
	lua_pushcclosure(L, glua::protect<factoryFor>, 2);
	lua_setfield(L, -2, meta->className());
}


template<typename T>
QObject* ctorFor() {
	return new T;
}

template<typename T>
static void registerClassT(lua_State* L) {
	registerClass(L, &T::staticMetaObject, ctorFor<T>);
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
	registerClassT<QMainWindow>(L);
	lua_setglobal(L, "gui");
}

}