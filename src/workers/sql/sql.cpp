#include "radapter.hpp"
#include "builtin.hpp"
#include <QSql>
#include <QSqlDatabase>
#include <QSqlError>

namespace radapter::sql {

struct ServerConfig {
    QString type;
    QString db;
    optional<QString> user;
    optional<QString> pass;
    optional<uint16_t> port;
};
DESCRIBE(ServerConfig, &_::type, &_::db, &_::user, &_::pass, &_::port)

class SqlWorker : public Worker {
    Q_OBJECT

    QSqlDatabase db;
    ServerConfig config;
public:
    SqlWorker(QVariant arg, Instance* inst) : Worker(inst, "sql") {
        Parse(config, arg);
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
};

}


void radapter::builtin::sql(radapter::Instance* inst) {
    // Wort in progress!
}

#include "sql.moc"
