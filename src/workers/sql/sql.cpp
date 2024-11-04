#include "radapter.hpp"
#include "builtin.hpp"
#include <QSql>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

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
    SqlConfig config;
public:
    SqlWorker(QVariantList args, Instance* inst) : Worker(inst, "sql") {
        Parse(config, args.value(0));
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
            throw Err("Could not open db {}:{} => {}",
                      config.type, config.db, err.text());
        }
    }
    QVariant Exec(QVariantList const& args) {
        auto argc = args.size();
        if (argc == 0) {
            throw Err("Expected at least 2 arguments");
        }
        QString raw = args.value(0).toString();
        QVariantList binds = args.value(1).toList();
        QSqlQuery q(db);
        if (!q.prepare(raw)) {
            throw Err("Could not prepare {} => {}", raw, q.lastError().text());
        }
        int idx = 0;
        for (auto& a: binds) {
            q.bindValue(idx++, std::move(a));
        }
        if (!q.exec()) {
            throw Err("Could not execute {} => {}", raw, q.lastError().text());
        }
        QVariantList result;
        Debug("{}", raw);
        while(q.next()) {
            QVariantList nested;
            auto c = q.record().count();
            nested.reserve(c);
            for (auto i = 0; i < c; ++i) {
                nested.push_back(q.record().value(i));
            }
            result.push_back(nested);
        }
        return result;
    }
    void OnMsg(QVariant const&) {
        // TODO: add way to describe schema and append msgs to archive
    }
};

}


void radapter::builtin::sql(radapter::Instance* inst) {
    using namespace radapter::sql;
    inst->RegisterWorker<SqlWorker>("Sql", {
        {"Exec", AsExtraMethod<&SqlWorker::Exec>}
    });
    inst->RegisterSchema<SqlConfig>("Sql");
}

#include "sql.moc"
