#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QWebSocketServer>
#include <QWebSocket>
#include <QFile>
#include <QSslKey>
#include <QSslCertificate>
#include <QTimer>
#include <qmetaobject.h>
#include <set>
#include <json_view/json_view.hpp>
#include <json_view/parse.hpp>
#include <json_view/dump.hpp>

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

enum WsProtocol {
    json,
    msgpack,
};

RAD_DESCRIBE(WsProtocol) {
    MEMBER("json", json);
    MEMBER("msgpack", msgpack);
}

enum WsCompression {
    zlib,
};

RAD_DESCRIBE(WsCompression) {
    MEMBER("zlib", zlib);
}

struct WsConfig {
    WithDefault<QString> origin = "radapter";
    WithDefault<string> cert_file = "";
    WithDefault<string> key_file = "";
    optional<WsCompression> compression;
    WithDefault<WsProtocol> protocol = json;
    WithDefault<int> compression_level = -1;
};

RAD_DESCRIBE(WsConfig) {
    RAD_MEMBER(origin);
    RAD_MEMBER(cert_file);
    RAD_MEMBER(key_file);
    RAD_MEMBER(compression);
    RAD_MEMBER(protocol);
    RAD_MEMBER(compression_level);
}

struct WsClientConfig : WsConfig {
    QString url;

    WithDefault<unsigned> reconnect_timeout = 10000u;
};

RAD_DESCRIBE(WsClientConfig) {
    PARENT(WsConfig);
    RAD_MEMBER(url);
    RAD_MEMBER(reconnect_timeout);
}

struct WsServerConfig : WsConfig {
    uint16_t port;

    WithDefault<string> host = "0.0.0.0";
};

RAD_DESCRIBE(WsServerConfig) {
    PARENT(WsConfig);
    RAD_MEMBER(port);
    RAD_MEMBER(host);
}

using namespace jv;


static bool isBinary(WsConfig const& config) {
    return bool(config.compression) || config.protocol != json;
}

static QByteArray prepareMsg(WsConfig const& config, QVariant const& _state) {
    QByteArray toSend;
    {
        jv::DefaultArena alloc;
        auto j = JsonView::From(_state, alloc);
        membuff::StringOut<QByteArray> buff;
        if (config.protocol == msgpack) {
            DumpMsgPackInto(buff, j);
        } else {
            DumpJsonInto(buff, j);
        }
        toSend = buff.Consume();
    }
    if (config.compression && *config.compression == zlib) {
        toSend = qCompress(toSend);
    }
    return toSend;
}


static QVariant recvFrom(QWebSocket* sock, Worker* self, WsConfig const& config, QByteArray msg) {
    QVariant fromClient;
    {
        DefaultArena alloc;
        JsonView recv;
        try {
            if (msg.isEmpty()) throw Err("Empty msg");
            if (config.compression) {
                if (*config.compression == zlib) {
                    msg = qUncompress(msg);
                    if (msg.isEmpty()) throw Err("Could not uncompress using zlib");
                }
            }
            if (config.protocol == msgpack) {
                recv = ParseMsgPackInPlace(msg.data(), size_t(msg.size()), alloc);
            } else {
                recv = ParseJsonInPlace(msg.data(), size_t(msg.size()), alloc);
            }
        } catch (std::exception& e) {
            self->Error("{}: Error receiving from ({}:{}) => {}",
                  self->objectName(), sock->peerAddress().toString(),
                  sock->peerPort(), e.what());
            return {};
        }
        fromClient = recv.Get<QVariant>();
    }
    return fromClient;
}

class Server : public Worker {
    Q_OBJECT

    WsServerConfig config;
    QWebSocketServer* server = nullptr;
    std::set<QWebSocket*> socks;
public:
    Server(QVariantList args, Instance* inst) :
        Worker(inst, "ws_server")
    {
        Parse(config, args.value(0));
        setObjectName(QString("Server(%1:%2/%3)")
                      .arg(config.host.value.c_str())
                      .arg(config.port)
                      .arg(config.origin.value));
        optional<QSslConfiguration> ssl;
        if (config.cert_file.value.size() || config.key_file.value.size()) {
            ssl = CreateSslConfiguration(config.cert_file, config.key_file);
        }
        auto mode = ssl ? QWebSocketServer::SecureMode : QWebSocketServer::NonSecureMode;
        server = new QWebSocketServer(config.origin, mode, this);
        if (ssl) {
            server->setSslConfiguration(*ssl);
        }
        if (!server->listen(QHostAddress(QString::fromStdString(config.host)), config.port)) {
            throw Err("{}: could not listen on: {}:{}", objectName(), config.host.value, config.port);
        }
        Info("{}: listening on {}:{}", objectName(), config.host.value, config.port);
        connect(server, &QWebSocketServer::newConnection, this, [this]{
            while(server->hasPendingConnections()) {
                accept(server->nextPendingConnection());
            }
        });
    }

    void OnMsg(QVariant const& msg) override {
        if (!msg.isValid()) return;
        auto toSend = prepareMsg(config, msg);
        if (isBinary(config)) {
            for (auto& cli: socks) {cli->sendBinaryMessage(toSend);}
        } else {
            auto str = QString::fromUtf8(toSend);
            for (auto& cli: socks) {cli->sendTextMessage(str);}
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
            auto parsed = recvFrom(sock, this, config, msg.toUtf8());
            emit SendMsg(parsed);
        });
        connect(sock, &QWebSocket::binaryMessageReceived, this, [=](QByteArray const& msg){
            auto parsed = recvFrom(sock, this, config, msg);
            emit SendMsg(parsed);
        });
        connect(sock, &QWebSocket::destroyed, this, [=]{
            socks.erase(sock);
        });
        socks.insert(sock);
    }
};

class Client : public Worker {
    Q_OBJECT

    WsClientConfig config;
    QWebSocket* sock;
public:
    Client(QVariantList const& args, Instance* inst) :
        Worker(inst, "ws_client")
    {
        Parse(config, args.value(0));
        setObjectName(QString("Client(%1:%2/%3)").arg(config.url));
        optional<QSslConfiguration> ssl;
        if (config.cert_file.value.size() || config.key_file.value.size()) {
            ssl = CreateSslConfiguration(config.cert_file, config.key_file);
        }
        sock = new QWebSocket(config.origin, QWebSocketProtocol::VersionLatest, this);
        if (ssl) {
            sock->setSslConfiguration(*ssl);
        }
        connect(sock, &QWebSocket::stateChanged, this, [=](auto state){
            const auto st = QMetaEnum::fromType<decltype(state)>().valueToKey(state);
            Info("{}: New state: {}", objectName(), st);
            emit SendEvent(QVariantMap{{"state", st}});
            if (state == QAbstractSocket::UnconnectedState) {
                QTimer::singleShot(config.reconnect_timeout, this, [=]{
                    sock->open(QUrl(config.url));
                });
            }
        });
        connect(sock, &QWebSocket::binaryMessageReceived, this, [=](QByteArray const& msg){
            auto m = recvFrom(sock, this, config, msg);
            if (m.isValid()) emit SendMsg(m);
        });
        connect(sock, &QWebSocket::textMessageReceived, this, [=](QString const& msg){
            auto m = recvFrom(sock, this, config, msg.toUtf8());
            if (m.isValid()) emit SendMsg(m);
        });
        sock->open(QUrl(config.url));
    }
    void OnMsg(QVariant const& msg) override {
        auto prepped = prepareMsg(config, msg);
        if (isBinary(config)) {
            sock->sendBinaryMessage(prepped);
        } else {
            sock->sendTextMessage(QString::fromUtf8(prepped));
        }
    }
};

}

void radapter::builtin::workers::websocket(radapter::Instance* inst) {
    inst->RegisterWorker<ws::Server>("WebsocketServer");
    inst->RegisterSchema<ws::WsServerConfig>("WebsocketServer");
    inst->RegisterWorker<ws::Client>("WebsocketClient");
    inst->RegisterSchema<ws::WsClientConfig>("WebsocketClient");
}

#include "websocket.moc"
