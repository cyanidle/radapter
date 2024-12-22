#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QSql>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSemaphore>
#include <future/future.hpp>
#include <qthread.h>
#include "async_helpers.hpp"

namespace radapter::sql {

struct SqlConfig {
    QString type;
    QString db;
    optional<QString> user;
    optional<QString> pass;
    optional<uint16_t> port;
};

DESCRIBE("radapter::ws::SqlConfig", SqlConfig, void) {
    MEMBER("type", &_::type);
    MEMBER("db", &_::db);
    MEMBER("user", &_::user);
    MEMBER("pass", &_::pass);
    MEMBER("port", &_::port);
}

using namespace fut;

class SqlWorker final: public Worker {
    Q_OBJECT

    QSqlDatabase db;
    QThread* thread{};
    QObject* other_thread_context{};
    SqlConfig config;
public:
    ~SqlWorker() override {
        if (other_thread_context)
            other_thread_context->deleteLater();
        thread->quit();
        thread->wait();
    }
    SqlWorker(QVariantList args, Instance* inst) : Worker(inst, "sql") {
        Parse(config, args.value(0));
        thread = new QThread(this);
        std::exception_ptr initException;
        QSemaphore sema;
        connect(thread, &QThread::started, [&]{
            try {
                db = QSqlDatabase::addDatabase(config.type);
                db.setDatabaseName(config.db);
                if (config.port) {
                    db.setPort(*config.port);
                }
                if (config.user) {
                    db.setUserName(*config.user);
                }
                if (config.pass) {
                    db.setPassword(*config.pass);
                }
                if (!db.open()) {
                    auto err = db.lastError();
                    throw Err("Could not open db '{}:{}' => '{}'",
                              config.type, config.db, err.text());
                }
                other_thread_context = new QObject();
            } catch (...) {
                initException = std::current_exception();
            }
            sema.release();
        });
        thread->start();
        sema.acquire();
        if (initException) {
            std::rethrow_exception(initException);
        }
    }

    void doExec(SharedPromise<QVariantList>& promise, QString& raw, QVariantList& binds) {
        QString error;
        QVariantList result;
        try {
            QSqlQuery q(db);
            if (!q.prepare(raw)) {
                throw Err("Could not prepare '{}' => '{}'", raw, q.lastError().text());
            }
            int idx = 0;
            for (auto& a: binds) {
                q.bindValue(idx++, std::move(a));
            }
            if (!q.exec()) {
                throw Err("Could not execute '{}' => '{}'", raw, q.lastError().text());
            }
            while(q.next()) {
                QVariantList nested;
                auto c = q.record().count();
                nested.reserve(c);
                for (auto i = 0; i < c; ++i) {
                    nested.push_back(q.record().value(i));
                }
                result.push_back(nested);
            }
            QMetaObject::invokeMethod(this, [MV(promise), MV(result)]() mutable {
                promise(std::move(result));
            });
        } catch (...) {
            QMetaObject::invokeMethod(this, [MV(promise), exc = std::current_exception()]() mutable {
                promise(std::move(exc));
            });
        }
    }

    // 1: Exec(cmd, function (ok, err) ... end) -> nil
    // 1a: Exec(cmd) -> async thunk with (ok, err)
    // 2: Exec(cmd, params, function (ok, err) ... end) -> nil
    // 2a: Exec(cmd, params) -> async thunk with (ok, err)
    QVariant Exec(QVariantList const& args) {
        auto argc = args.size();
        if (argc == 0) {
            throw Err("Expected at least 2 arguments");
        }
        QString raw = args.value(0).toString();
        QVariant secondArg = args.value(1);
        QVariantList binds;
        LuaFunction cb;
        if (!(cb = secondArg.value<LuaFunction>())) {
            binds = secondArg.toList();
            cb = args.value(2).value<LuaFunction>();
        }
        SharedPromise<QVariantList> promise;
        Future<QVariantList> future = promise.GetFuture();
        QMetaObject::invokeMethod(other_thread_context, [this, MV(promise), MV(raw), MV(binds)]() mutable {
            doExec(promise, raw, binds);
        });
        if (args.size() == 1 || (args.size() == 2 && !cb)) {
            //async signature
            return makeLuaPromise(this, future);
        } else {
            resolveLuaCallback(this, future, cb);
            return {};
        }
    }

    void OnMsg(QVariant const&) override {
        // TODO: add way to describe schema and append msgs to archive
    }
};

}


void radapter::builtin::workers::sql(radapter::Instance* inst) {
    using namespace radapter::sql;
    inst->RegisterWorker<SqlWorker>("Sql", {
        {"Exec", AsExtraMethod<&SqlWorker::Exec>}
    });
    inst->RegisterSchema<SqlConfig>("Sql");
}

#include "sql.moc"
