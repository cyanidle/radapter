#pragma once

#include "radapter/worker.hpp"
#include "glua/glua.hpp"
#include <QPointer>

namespace radapter {

// object which represents worker inside lua
struct WorkerImpl {
    lua_State* L;
    QPointer<radapter::Worker> self{};
    QMetaObject::Connection conn{};
    QVariant currentSender = {};
    LuaValue listeners = {};
    LuaValue evListeners = {};

    ~WorkerImpl() {
        if (self) {
            QObject::disconnect(conn);
            self->deleteLater();
        }
    }
};

DESCRIBE("radapter::WorkerImpl", WorkerImpl, void) {}

}
