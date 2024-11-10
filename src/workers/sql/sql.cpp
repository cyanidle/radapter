#include "radapter.hpp"
#include "builtin.hpp"
#include <QSql>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSemaphore>
#include <qthread.h>

namespace radapter::sql {

struct SqlConfig {
    QString type;
    QString db;
    optional<QString> user;
    optional<QString> pass;
    optional<uint16_t> port;
};
DESCRIBE(SqlConfig, &_::type, &_::db, &_::user, &_::pass, &_::port)

class SqlWorker final: public Worker {
    Q_OBJECT

    QSqlDatabase db;
    QThread* thread{};
    QObject* context{};
    SqlConfig config;
public:
    ~SqlWorker() override {
        if (context) delete context;
        thread->quit();
        thread->wait();
    }
    SqlWorker(QVariantList args, Instance* inst) : Worker(inst, "sql") {
        Parse(config, args.value(0));
        thread = new QThread(this);
        std::exception_ptr initException;
        QSemaphore sema;
        connect(thread, &QThread::started, thread, [&, this]{
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
                context = new QObject();
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
    QVariant Exec(QVariantList const& args) {
        auto argc = args.size();
        if (argc == 0) {
            throw Err("Expected at least 2 arguments");
        }
        QString raw = args.value(0).toString();
        auto secondArg = args.value(1);
        QVariantList binds;
        LuaFunction cb;
        if (!(cb = secondArg.value<LuaFunction>())) {
            binds = secondArg.toList();
            cb = args.value(2).value<LuaFunction>();
        }
        auto do_exec = [=]{
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
                Debug("'{}'", raw);
                while(q.next()) {
                    QVariantList nested;
                    auto c = q.record().count();
                    nested.reserve(c);
                    for (auto i = 0; i < c; ++i) {
                        nested.push_back(q.record().value(i));
                    }
                    result.push_back(nested);
                }
            } catch (std::exception& e) {
                error = e.what();
            }
            QMetaObject::invokeMethod(this, [=]{
                if (cb) cb({result, error.isEmpty() ? QVariant() : error});
                else if (!error.isEmpty()) Error("Unhandled error in sql: {}", error);
            });
        };
        QMetaObject::invokeMethod(context, do_exec);
        return {};
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
