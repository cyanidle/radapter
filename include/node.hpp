#pragma once

#include "json.hpp"
#include <QObject>

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
