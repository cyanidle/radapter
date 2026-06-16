#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include "workers/wire.hpp"
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <map>

namespace radapter::local {

// `socket` is the local socket / named-pipe name (the base `name` is the worker name).
struct ServerConfig : WorkerConfig {
    QString socket;
    WithDefault<wire::Protocol> protocol = wire::json;
    std::optional<wire::Compression> compression;
    WithDefault<bool> per_client = false;   // route per connection (see OnMsg)
};
RAD_DESCRIBE(ServerConfig) {
    PARENT(WorkerConfig);
    RAD_MEMBER(socket);
    RAD_MEMBER(protocol);
    RAD_MEMBER(compression);
    RAD_MEMBER(per_client);
}

struct ClientConfig : WorkerConfig {
    QString socket;
    WithDefault<wire::Protocol> protocol = wire::json;
    std::optional<wire::Compression> compression;
    WithDefault<unsigned> reconnect_timeout = 300u;
};
RAD_DESCRIBE(ClientConfig) {
    PARENT(WorkerConfig);
    RAD_MEMBER(socket);
    RAD_MEMBER(protocol);
    RAD_MEMBER(compression);
    RAD_MEMBER(reconnect_timeout);
}

static QString stateName(QLocalSocket::LocalSocketState st) {
    switch (st) {
    case QLocalSocket::UnconnectedState: return QStringLiteral("UnconnectedState");
    case QLocalSocket::ConnectingState:  return QStringLiteral("ConnectingState");
    case QLocalSocket::ConnectedState:   return QStringLiteral("ConnectedState");
    case QLocalSocket::ClosingState:     return QStringLiteral("ClosingState");
    }
    return QStringLiteral("Unknown");
}

// Local-socket server. With per_client = false (default) it broadcasts: inbound
// emits the raw payload, OnMsg sends to every client. With per_client = true it
// routes per connection: inbound arrives as { "<clientId>" = payload } and a map
// keyed by clientId targets those clients (unknown keys fall back to broadcast).
// Events: { connected = id } / { disconnected = id }.
class Server : public Worker {
    Q_OBJECT

    ServerConfig config;
    QLocalServer* server;
    std::map<QString, QLocalSocket*> socks;
    std::map<QLocalSocket*, QByteArray> bufs;
    unsigned counter = 0;
public:
    Server(ServerConfig conf, Instance* inst) :
        Worker(inst, EnsureName(conf, conf.socket), "local_server")
    {
        config = std::move(conf);
        QLocalServer::removeServer(config.socket);   // clear a stale socket file
        server = new QLocalServer(this);
        if (!server->listen(config.socket)) {
            Raise("could not listen on local socket {}: {}",
                  config.socket, server->errorString());
        }
        Info("listening on local socket {}", config.socket);
        connect(server, &QLocalServer::newConnection, this, [this]{
            while (server->hasPendingConnections()) {
                accept(server->nextPendingConnection());
            }
        });
    }

    void accept(QLocalSocket* sock) {
        auto id = QString::number(++counter);
        sock->setParent(this);
        socks[id] = sock;
        bufs[sock] = {};
        Info("new client {}", id);
        emit SendEvent(QVariantMap{{"connected", id}});
        auto onRead = [this, sock, id]{
            auto& buf = bufs[sock];
            buf += sock->readAll();
            wire::DrainFrames(buf, config.protocol.value, config.compression,
                [this, &id](QVariant const& v){
                    if (config.per_client.value) emit SendMsg(QVariantMap{{id, v}});
                    else emit SendMsg(v);
                },
                [this](QString e){ Error("receive parse error: {}", e); });
        };
        connect(sock, &QLocalSocket::readyRead, this, onRead);
        connect(sock, &QLocalSocket::disconnected, this, [this, sock, id]{
            Warn("client disconnected {}", id);
            emit SendEvent(QVariantMap{{"disconnected", id}});
            socks.erase(id);
            bufs.erase(sock);
            sock->deleteLater();
        });
        // data may have arrived before readyRead was connected (the client can write
        // the instant it sees ConnectedState); QLocalSocket won't re-signal it
        if (sock->bytesAvailable() > 0) onRead();
    }

    void OnMsg(QVariant const& msg) override {
        if (!msg.isValid()) return;
        if (config.per_client.value && msg.type() == QVariant::Map) {
            auto m = msg.toMap();
            bool targeted = false;
            for (auto it = m.begin(); it != m.end(); ++it) {
                auto sit = socks.find(it.key());
                if (sit != socks.end()) {
                    targeted = true;
                    sit->second->write(wire::Frame(config.protocol.value, config.compression, it.value()));
                }
            }
            if (targeted) return;
        }
        auto framed = wire::Frame(config.protocol.value, config.compression, msg);
        for (auto& [id, s] : socks) {
            (void)id;
            s->write(framed);
        }
    }

    void Destroy() override {
        if (server) server->close();
        Worker::Destroy();
    }
};

// Connects to a LocalServer by name and reconnects on drop. Inbound messages are
// framed and written; received frames are emitted on the data channel. Connection
// state is reported on the event channel as { state = "ConnectedState" } etc.
class Client : public Worker {
    Q_OBJECT

    ClientConfig config;
    QLocalSocket* sock;
    QByteArray buf;
public:
    Client(ClientConfig conf, Instance* inst) :
        Worker(inst, EnsureName(conf, conf.socket), "local_client")
    {
        config = std::move(conf);
        sock = new QLocalSocket(this);
        connect(sock, &QLocalSocket::stateChanged, this, [this](QLocalSocket::LocalSocketState st){
            auto name = stateName(st);
            Info("state: {}", name);
            // a local connect completes synchronously inside connectToServer(), so the
            // state change arrives before the creating script subscribed to .events AND
            // before the device is fully open. Deliver the event on the next tick so a
            // handler that replies (e.g. sends on ConnectedState) sees an open socket.
            QMetaObject::invokeMethod(this, [this, name]{
                emit SendEvent(QVariantMap{{"state", name}});
            }, Qt::QueuedConnection);
            if (st == QLocalSocket::UnconnectedState) {
                QTimer::singleShot(int(config.reconnect_timeout.value), this, [this]{
                    sock->connectToServer(config.socket);
                });
            }
        });
        connect(sock, &QLocalSocket::readyRead, this, [this]{
            buf += sock->readAll();
            wire::DrainFrames(buf, config.protocol.value, config.compression,
                [this](QVariant const& v){ emit SendMsg(v); },
                [this](QString e){ Error("receive parse error: {}", e); });
        });
        sock->connectToServer(config.socket);
    }

    void OnMsg(QVariant const& msg) override {
        sock->write(wire::Frame(config.protocol.value, config.compression, msg));
    }
};

}

void radapter::builtin::workers::local(radapter::Instance* inst) {
    inst->RegisterWorker<local::Server>("LocalServer");
    inst->RegisterSchema<local::ServerConfig>("LocalServer");
    inst->RegisterWorker<local::Client>("LocalClient");
    inst->RegisterSchema<local::ClientConfig>("LocalClient");
}

#include "local.moc"
