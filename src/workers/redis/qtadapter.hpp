#pragma once
#include "radapter.hpp"

struct redisAsyncContext;
class QSocketNotifier;
class QtRedisAdapter : public QObject
{
    Q_OBJECT
public:
    QtRedisAdapter(QObject * parent = nullptr);
    ~QtRedisAdapter() override;
    int SetContext(redisAsyncContext * ac);
private slots:
    void read();
    void write();
private:
    struct Impl;
    void addRead();
    void delRead();
    void addWrite();
    void delWrite();
    void cleanup();

    redisAsyncContext * m_ctx = {};
    QSocketNotifier * m_read = {};
    QSocketNotifier * m_write = {};
};
