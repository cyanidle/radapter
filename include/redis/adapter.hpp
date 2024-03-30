#include <QSocketNotifier>
#include "./async.h"

class QtRedisAdapter : public QObject
{
    Q_OBJECT
public:
    int SetContext(redisAsyncContext *ac) {
        if (ac->ev.data != NULL) {
            return REDIS_ERR;
        }
        m_ctx = ac;
        m_ctx->ev.data = this;
        m_ctx->ev.addRead = [](void* a){if(a)static_cast<QtRedisAdapter*>(a)->addRead();};
        m_ctx->ev.delRead = [](void* a){if(a)static_cast<QtRedisAdapter*>(a)->delRead();};
        m_ctx->ev.addWrite = [](void* a){if(a)static_cast<QtRedisAdapter*>(a)->addWrite();};
        m_ctx->ev.delWrite = [](void* a){if(a)static_cast<QtRedisAdapter*>(a)->delWrite();};
        m_ctx->ev.cleanup = [](void* a){if(a)static_cast<QtRedisAdapter*>(a)->cleanup();};
        return REDIS_OK;
    }

    void read()
    {
        redisAsyncHandleRead(m_ctx);
    }

    void write()
    {
        redisAsyncHandleWrite(m_ctx);
    }

    void addRead() {
        if (m_read) return;
        m_read = new QSocketNotifier(m_ctx->c.fd, QSocketNotifier::Read, this);
        connect(m_read, &QSocketNotifier::activated, this, &QtRedisAdapter::read);
    }

    void delRead() {
        if (!m_read) return;
        delete m_read;
        m_read = nullptr;
    }

    void addWrite() {
        if (m_write) return;
        m_write = new QSocketNotifier(m_ctx->c.fd, QSocketNotifier::Write, this);
        connect(m_write, &QSocketNotifier::activated, this, &QtRedisAdapter::write);
    }

    void delWrite() {
        if (!m_write) return;
        delete m_write;
        m_write = nullptr;
    }

    void cleanup() {
        delRead();
        delWrite();
    }

    QtRedisAdapter(QObject* parent) :
        QObject{parent}
    {}

    ~QtRedisAdapter()
    {
        if (m_ctx != nullptr) {
            m_ctx->ev.data = NULL;
        }
    }
private:
    redisAsyncContext * m_ctx = {};
    QSocketNotifier * m_read = {};
    QSocketNotifier * m_write = {};
};
