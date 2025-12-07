#include "qtadapter.hpp"
#include <QSocketNotifier>
#include "redis_inc.h"

QtRedisAdapter::QtRedisAdapter(QObject* parent) :
    QObject{parent}
{}

QtRedisAdapter::~QtRedisAdapter()
{
    if (m_ctx != nullptr) {
        m_ctx->ev.data = NULL;
    }
}

struct QtRedisAdapter::Impl
{
    static void RedisQtDelRead(void *adapter)
    {
        auto a = static_cast<QtRedisAdapter*>(adapter);
        a->delRead();
    }

    static void RedisQtAddWrite(void *adapter)
    {
        auto a = static_cast<QtRedisAdapter*>(adapter);
        a->addWrite();
    }

    static void RedisQtDelWrite(void *adapter)
    {
        auto a = static_cast<QtRedisAdapter*>(adapter);
        a->delWrite();
    }

    static void RedisQtCleanup(void *adapter)
    {
        auto a = static_cast<QtRedisAdapter*>(adapter);
        a->cleanup();
    }

    static void RedisQtAddRead(void *adapter)
    {
        auto a = static_cast<QtRedisAdapter*>(adapter);
        a->addRead();
    }
};

int QtRedisAdapter::SetContext(redisAsyncContext *ac) {
    if (ac->ev.data != NULL) {
        return REDIS_ERR;
    }
    m_ctx = ac;
    m_ctx->ev.data = this;
    m_ctx->ev.addRead = Impl::RedisQtAddRead;
    m_ctx->ev.delRead = Impl::RedisQtDelRead;
    m_ctx->ev.addWrite = Impl::RedisQtAddWrite;
    m_ctx->ev.delWrite = Impl::RedisQtDelWrite;
    m_ctx->ev.cleanup = Impl::RedisQtCleanup;
    return REDIS_OK;
}

void QtRedisAdapter::read()
{
    redisAsyncHandleRead(m_ctx);
}

void QtRedisAdapter::write()
{
    redisAsyncHandleWrite(m_ctx);
}

void QtRedisAdapter::addRead() {
    if (m_read) return;
    m_read = new QSocketNotifier(qintptr(m_ctx->c.fd), QSocketNotifier::Read, this);
    connect(m_read, &QSocketNotifier::activated, this, &QtRedisAdapter::read);
}

void QtRedisAdapter::delRead() {
    if (!m_read) return;
    delete m_read;
    m_read = nullptr;
}

void QtRedisAdapter::addWrite() {
    if (m_write) return;
    m_write = new QSocketNotifier(qintptr(m_ctx->c.fd), QSocketNotifier::Write, this);
    connect(m_write, &QSocketNotifier::activated, this, &QtRedisAdapter::write);
}

void QtRedisAdapter::delWrite() {
    if (!m_write) return;
    delete m_write;
    m_write = nullptr;
}

void QtRedisAdapter::cleanup() {
    delRead();
    delWrite();
}
