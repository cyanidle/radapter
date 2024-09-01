#include "radapter.hpp"
#include "builtin.hpp"
#include <QTimer>
#include "fmt/compile.h"
#include "qtadapter.hpp"
#include "redis_client.hpp"
#include <qcoreapplication.h>

namespace radapter::redis {

struct CacheConfig : Config {
    optional<string> hash_key;
    WithDefault<unsigned> update_rate = 500u;
};
DESCRIBE_INHERIT(redis::CacheConfig, Config, &_::hash_key, &_::update_rate)

enum StreamStart {
    from_persistent_id,
    from_top,
    from_start,
};
DESCRIBE(StreamStart, from_persistent_id, from_top, from_start)

struct StreamConfig : Config {
    string stream_key;
    WithDefault<StreamStart> start_from = from_persistent_id;
    WithDefault<unsigned> stream_size = 1'000'000u;
    WithDefault<unsigned> block_timeout = 30'000u;
    WithDefault<unsigned> entries_per_read = 1'000u;
    WithDefault<bool> read_enabled = true;
    WithDefault<bool> write_enabled = true;
    WithDefault<string> persistent_prefix = "__radapter";
};
DESCRIBE_INHERIT(redis::StreamConfig, Config,
                 &_::stream_key, &_::start_from, &_::stream_size,
                 &_::block_timeout, &_::entries_per_read,
                 &_::read_enabled, &_::write_enabled)

class Cache : public Worker
{
    Q_OBJECT

    CacheConfig config;
    Client* client = nullptr;
    QVariant state;
public:
    Cache(QVariantList args, Instance* inst) : Worker(inst, "redis")
    {
        Parse(config, args.value(0));
        client = new Client(config, this);
        setObjectName(QString("%1_hash(%2)")
                          .arg(client->objectName())
                          .arg(config.hash_key ? config.hash_key->c_str() : "-"));
        client->setObjectName(objectName());
        auto poller = new QTimer(this);
        poller->callOnTimeout(this, &Cache::poll);
        connect(client, &Client::ConnectedChanged, this, [=](bool state){
            if (state && config.hash_key) {
                poller->start(int(config.update_rate));
            } else {
                poller->stop();
            }
        });
        client->Start();
    }
    void poll() {
        assert(config.hash_key);
        auto cmd = fmt::format("HGETALL {}", *config.hash_key);
        client->Execute(cmd, [this](QVariant resp, std::exception_ptr exc){
            if (exc) {
                try {
                    std::rethrow_exception(exc);
                } catch (std::exception& e) {
                    Error("{}: Could not read hash {} => {}", objectName(), *config.hash_key, e.what());
                }
            } else {
                if (resp.type() != QVariant::List) {
                    Error("{}: error reading all keys: {}", objectName(), resp.toString());
                    return;
                }
                auto list = resp.toList();
                auto size = list.size();
                size ^= size & 1; //make even
                FlatMap flat;
                for (auto i = 0; i < size; i += 2) {
                    flat.push_back({list[i].toString().toStdString(), list[i+1]});
                }
                QVariant unflat;
                Unflatten(unflat, flat);
                QVariant diff;
                if (MergePatch(state, unflat, &diff)) {
                    emit SendMsg(diff);
                }
            }
        });

    }
    void OnMsg(QVariant const& msg) override {
        if (!config.hash_key) {
            Error("{}: cannot handle msg: hash_key not set!", objectName());
            return;
        }
        FlatMap flat;
        Flatten(flat, msg);
        string cmd = "HMSET " + *config.hash_key;
        for (auto& [k, v]: flat) {
            cmd += fmt::format(FMT_COMPILE(" {} {}"), k, v.toString().toStdString());
        }
        client->Execute(std::move(cmd), [this](QVariant resp, std::exception_ptr except){
            if (except) {
                try {
                    std::rethrow_exception(except);
                } catch (std::exception& e) {
                    Error("{}: error writing: {}", objectName(), e.what());
                }
            } else if (resp != "OK") {
                Error("{}: non-ok responce: {}", objectName(), resp.toString());
            }
        });
    }
    QVariant Execute(QVariantList args) {
        auto cmd = args.value(0).toString();
        auto cb = args.value(1).value<LuaFunction>();
        if (cmd.isEmpty()) throw Err("cmd string as argument #1 expected");
        client->Execute(cmd.toStdString(), [this, cb = std::move(cb)](QVariant res, std::exception_ptr exc){
            if (exc) {
                try {
                    std::rethrow_exception(exc);
                } catch (std::exception& e) {
                    if (cb) {
                        cb({{}, e.what()});
                    } else {
                        Warn("{}: unhandled exception: {}", objectName(), e.what());
                    }
                }
            } else if (cb) {
                cb({res, {}});
            }
        });
        return {};
    }
};

class Stream : public Worker
{
    Q_OBJECT

    StreamConfig config;
    Client* client = nullptr;
    Client* read_client = nullptr;
    string lastId;
public:
    void saveLastId() {
        auto cmd = fmt::format("SET {}:{}:last_id {}", config.persistent_prefix.value, config.stream_key, lastId);
        client->Execute(std::move(cmd), [this](QVariant, std::exception_ptr exc){
            if (exc) {
                try {
                    std::rethrow_exception(exc);
                } catch (std::exception& e) {
                    Error("{}: Could not save last id: {}", objectName(), e.what());
                }
            }
        });
    }
    void loadIdAndRead() {
        auto cmd = fmt::format("GET {}:{}:last_id", config.persistent_prefix.value, config.stream_key);
        client->Execute(std::move(cmd), [this](QVariant resp, std::exception_ptr exc){
            if (exc) {
                try {
                    std::rethrow_exception(exc);
                } catch (std::exception& e) {
                    Error("{}: Could not get last id: {}", objectName(), e.what());
                }
            } else {
                lastId = resp.toString().toStdString();
            }
            if (lastId.empty()) {
                Warn("{}: empty last id!", objectName());
                lastId = "0-0";
            }
            Info("{}: will start from persistent id: {}", objectName(), lastId);
            nextRead();
        });
    }
    Stream(QVariantList args, Instance* inst) : Worker(inst, "redis")
    {
        Parse(config, args.value(0));
        client = new Client(config, this);
        client->setObjectName(client->objectName() + QString("_stream(%1)").arg(config.stream_key.c_str()));
        setObjectName(client->objectName());
        client->Start();
        if (config.read_enabled) {
            read_client = new Client(config, this);
            read_client->setObjectName(objectName()+"_read");
            read_client->Start();
            connect(read_client, &Client::ConnectedChanged, this, [this](bool state){
                if (state) {
                    if (config.start_from == from_persistent_id) {
                        loadIdAndRead();
                    } else {
                        if (config.start_from == from_start) {
                            lastId = "0-0";
                        } else {
                            lastId = "$";
                        }
                        nextRead();
                    }
                }
            });
        }
    }
    void nextRead() {
        read_client->Execute(
            fmt::format("XREAD COUNT {} BLOCK {} STREAMS {} {}",
                        config.entries_per_read.value, config.block_timeout.value,
                        config.stream_key, lastId),
            [this](QVariant resp, std::exception_ptr exc){
                if (exc) {
                    try {
                        std::rethrow_exception(exc);
                    } catch (std::exception& e) {
                        Error("{}: error reading stream: {}", objectName(), e.what());
                        nextRead();
                        return;
                    }
                }
                parseReply(resp);
                nextRead();
            });
    }
    void parseReply(QVariant& resp) {
        if (resp.type() != QVariant::List) {
            Error("{}: error reading stream: {}", objectName(), resp.toString());
            return;
        }
        auto entries = resp.toList().value(0).toList().value(1).toList();
        for (auto& e: entries) {
            auto pair = e.toList();
            auto nextId = pair.value(0).toString().toStdString();
            if (!nextId.empty()) {
                lastId = std::move(nextId);
            }
            auto fields = pair.value(1).toList();
            auto size = fields.size();
            size ^= size & 1;
            FlatMap values;
            for (auto i = 0; i < size; i += 2) {
                values.push_back({fields[i].toString().toStdString(), fields[i+1]});
            }
            QVariant unflat;
            Unflatten(unflat, values);
            emit SendMsg(unflat);
        }
        saveLastId();
    }
    void OnMsg(QVariant const& msg) override {
        if (!config.write_enabled) {
            Warn("{}: writing is disabled", objectName());
            return;
        }
        FlatMap flat;
        Flatten(flat, msg);
        string cmd = fmt::format("XADD {} MAXLEN ~ {} *", config.stream_key, config.stream_size.value);
        for (auto& [k, v]: flat) {
            cmd += fmt::format(" {} {}", k, v.toString());
        }
        client->Execute(std::move(cmd), [this](QVariant, std::exception_ptr exc){
            if (exc) {
                try {
                    std::rethrow_exception(exc);
                } catch (std::exception& e) {
                    Error("{}: could not write stream: {}", objectName(), e.what());
                    return;
                }
            }
        });
    }
};

}

void radapter::builtin::redis(Instance* inst) {
    inst->RegisterWorker<redis::Cache>("RedisCache", {
        {"Execute", AsExtraMethod<&redis::Cache::Execute>},
    });
    inst->RegisterSchema("RedisCache", SchemaFor<redis::CacheConfig>);

    inst->RegisterWorker<redis::Stream>("RedisStream");
    inst->RegisterSchema("RedisStream", SchemaFor<redis::StreamConfig>);
}

#include "redis.moc"
