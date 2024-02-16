#pragma once

#include "json_view.hpp"
#include <QObject>
#include "lua.hpp"

namespace radapter
{


class Node : public QObject
{
    Q_OBJECT
public:
    explicit Node(QObject* parent = nullptr);
    static void Register(lua_State* L);
    ~Node() override;
signals:
    void Out(Json msg);
public slots:
    virtual void In(Json msg);
};




}
