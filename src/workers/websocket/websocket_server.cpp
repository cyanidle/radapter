#include "radapter.hpp"
#include "builtin.hpp"
#include <QJsonDocument>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QFile>
#include <QSslKey>
#include <QSslCertificate>
#include <QTimer>
#include <set>

namespace radapter::ws {

static QSslConfiguration CreateSslConfiguration(string cert_file, string key_file)
{
    auto fcert = QFile(QString::fromStdString(cert_file));
    if (!fcert.open(QIODevice::ReadOnly)) {
        throw Err("could not open certificate file: {}", cert_file);
    }
    auto fpkey = QFile(QString::fromStdString(key_file));
    if (!fpkey.open(QIODevice::ReadOnly)) {
        throw Err("could not open key file: {}", key_file);
    }

    auto cert = QSslCertificate(&fcert, QSsl::Pem);
    auto pkey = QSslKey(&fpkey, QSsl::Rsa, QSsl::Pem);

    fcert.close();
    fpkey.close();

    // TODO: Verify

    auto ssl = QSslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    ssl.setLocalCertificate(cert);
    ssl.setPrivateKey(pkey);

    return ssl;
}

struct ServerConfig {
    uint16_t port;

    WithDefault<string> host = "0.0.0.0";
    WithDefault<string> name = "radapter";
    WithDefault<unsigned> refresh_timeout = 10'000u;
    WithDefault<bool> print_msgs = false;
    WithDefault<string> cert_file = "";
    WithDefault<string> key_file = "";
};
DESCRIBE(ServerConfig, &_::host, &_::port, &_::name,
         &_::refresh_timeout, &_::print_msgs, &_::cert_file, &_::key_file)

class Server : public Worker {
    Q_OBJECT

    ServerConfig config;
    QWebSocketServer* server = nullptr;
    QVariant state;
    std::set<QWebSocket*> socks;
public:
    Server(QVariantList args, Instance* inst) : Worker(inst, "websocket"), server()
    {
        Parse(config, args.value(0));
        setObjectName(QString("Server(%1:%2/%3)")
                      .arg(config.host.value.c_str())
                      .arg(config.port)
                      .arg(config.name.value.c_str()));
        optional<QSslConfiguration> ssl;
        if (config.cert_file.value.size() || config.key_file.value.size()) {
            ssl = CreateSslConfiguration(config.cert_file, config.key_file);
        }
        auto mode = ssl ? QWebSocketServer::SecureMode : QWebSocketServer::NonSecureMode;
        server = new QWebSocketServer(QString::fromStdString(config.name), mode, this);
        if (ssl) {
            server->setSslConfiguration(*ssl);
        }
        if (!server->listen(QHostAddress(QString::fromStdString(config.host)), config.port)) {
            throw Err("{}: could not listen on: {}:{}", objectName(), config.host.value, config.port);
        }
        Info("{}: listening on {}:{}", objectName(), config.host.value, config.port);
        auto timer = new QTimer(this);
        timer->callOnTimeout(this, [this]{
            if (state.isValid()) {
                broadcast(state);
            }
        });
        timer->start(int(config.refresh_timeout));
        connect(server, &QWebSocketServer::newConnection, this, [this]{
            while(server->hasPendingConnections()) {
                accept(server->nextPendingConnection());
            }
        });
    }
    void OnMsg(QVariant const& msg) override {
        QVariant diff;
        if (MergePatch(state, msg, &diff)) {
            broadcast(diff);
        }
    }
    void accept(QWebSocket* sock) {
        sock->setParent(this);
        sock->setObjectName(QString("%1:%2").arg(sock->peerAddress().toString()).arg(sock->peerPort()));
        Info("{}: new client {}", objectName(), sock->objectName());
        connect(sock, &QWebSocket::disconnected, sock, [=]{
            Warn("{}: client disconnected {}", objectName(), sock->objectName());
            sock->deleteLater();
        });
        connect(sock, &QWebSocket::textMessageReceived, this, [=](QString const& msg){
            onClient(sock, msg);
        });
        connect(sock, &QWebSocket::destroyed, this, [=]{
            socks.erase(sock);
        });
        socks.insert(sock);
        sock->sendTextMessage(QString(QJsonDocument::fromVariant(state).toJson()));
    }
    void onClient(QWebSocket* sock, QString const& msg) {
        QJsonParseError err;
        auto json = QJsonDocument::fromJson(msg.toUtf8(), &err);
        if (err.error) {
            Error("{}: could not parse json from ({}:{}) => {} @ {}",
                    objectName(), sock->peerAddress().toString(), sock->peerPort(),
                    err.errorString(), err.offset);
            return;
        }
        if (config.print_msgs) {
            Debug("{} <== {}", objectName(), msg);
        }
        auto var = json.toVariant();
        emit SendMsg(var);
    }
    void broadcast(QVariant const& state) {
        auto toSend = QString(QJsonDocument::fromVariant(state).toJson());
        if (config.print_msgs) {
            Debug("{} ==> {}", objectName(), toSend);
        }
        for (auto cli: socks) {
            cli->sendTextMessage(toSend);
        }
    }
};


}

void radapter::builtin::websocket(radapter::Instance* inst) {
    inst->RegisterWorker<ws::Server>("WebsocketServer");
    inst->RegisterSchema("WebsocketServer", SchemaFor<ws::ServerConfig>);
}

#include "websocket_server.moc"
