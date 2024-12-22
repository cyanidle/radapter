#include "radapter/radapter.hpp"
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
DESCRIBE("redis::CacheConfig", CacheConfig, void) {
    PARENT(Config);
    MEMBER("hash_key", &_::hash_key);
    MEMBER("update_rate", &_::update_rate);
}

enum StreamStart {
    persistent_id,
    top,
    start,
};
DESCRIBE("redis::StreamStart", StreamStart, void) {
    MEMBER("persistent_id", persistent_id);
    MEMBER("top", top);
    MEMBER("start", start);
}

struct StreamConfig : Config {
    string stream_key;
    WithDefault<StreamStart> start_from = persistent_id;
    WithDefault<unsigned> stream_size = 1'000'000u;
    WithDefault<unsigned> block_timeout = 30'000u;
    WithDefault<unsigned> entries_per_read = 1'000u;
    WithDefault<bool> read_enabled = true;
    WithDefault<bool> write_enabled = true;
    WithDefault<string> instance_id = "_";
    WithDefault<string> persistent_prefix = "__radapter";
};
DESCRIBE("redis::StreamConfig", StreamConfig, void) {
    PARENT(Config);
    MEMBER("stream_key", &_::stream_key);
    MEMBER("start_from", &_::start_from);
    MEMBER("stream_size", &_::stream_size);
    MEMBER("block_timeout", &_::block_timeout);
    MEMBER("entries_per_read", &_::entries_per_read);
    MEMBER("read_enabled", &_::read_enabled);
    MEMBER("write_enabled", &_::write_enabled);
    MEMBER("instance_id", &_::instance_id);
    MEMBER("persistent_prefix", &_::persistent_prefix);
}

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
        connect(client, &Client::ConnectedChanged, this, [=](bool _state){
            if (_state && config.hash_key) {
                poller->start(int(config.update_rate));
            } else {
                poller->stop();
            }
        });
        client->Start();
    }
    void poll() {
        assert(config.hash_key);
        client->Execute({"HGETALL", *config.hash_key}, [this](QVariant resp, std::exception_ptr exc){
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
        RedisCmd cmd("HMSET");
        cmd.Arg(*config.hash_key);
        for (auto& [k, v]: flat) {
            cmd.Arg(k);
            cmd.Temp(v.toString().toStdString());
        }
        client->Execute(cmd, [this](QVariant resp, std::exception_ptr except){
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

    // 1: Exec(cmd, function (ok, err) ... end)
    // 2: Exec(cmd, {arg1, arg2, ...}, function (ok, err) ... end)
    QVariant Exec(QVariantList args) {
        QStringList rawcmd = args.value(0).toString().split(' ');
        RedisCmd cmd;
        for (auto& part: qAsConst(rawcmd)) {
            cmd.Temp(part.toStdString());
        }
        int funcIdx = 1;
        if (args.size() > 2) {
            funcIdx = 2;
            for (auto& arg: args.value(1).toList()) {
                cmd.Temp(arg.toString().toStdString());
            }
        }
        LuaFunction cb = args.value(funcIdx).value<LuaFunction>();
        client->Execute(cmd, [this, cb = std::move(cb)](QVariant res, std::exception_ptr exc) mutable {
            try {
                if (exc) std::rethrow_exception(exc);
                runCb(cb, true, std::move(res));
            } catch (std::exception& e) {
                runCb(cb, false, e.what());
            }
        });
        return {};
    }

    void runCb(LuaFunction& func, bool ok, QVariant result) {
        if (!func) {
            if (!ok) {
                Error("Unhandled error: {}", result.toString());
            }
            return;
        }
        auto args = ok
                        ? QVariantList{std::move(result), {}}
                        : QVariantList{{}, std::move(result)};
        try {
            func(args);
        } catch (std::exception& e) {
            Error("Error in callback: {}", e.what());
        }
    }
};

class Stream : public Worker
{
    Q_OBJECT

    StreamConfig config;
    Client* client = nullptr;
    Client* read_client = nullptr;
    string lastId;

    string lastIdKey;
public:
    void saveLastId() {
        client->Execute({"SET", lastIdKey, lastId}, [this](QVariant, std::exception_ptr exc){
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
        client->Execute({"GET", lastIdKey}, [this](QVariant resp, std::exception_ptr exc){
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
        lastIdKey = fmt::format(
            "{}:{}:{}:last_id",
            config.persistent_prefix.value,
            config.instance_id.value,
            config.stream_key);
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
                    if (config.start_from == persistent_id) {
                        loadIdAndRead();
                    } else {
                        if (config.start_from == start) {
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
        read_client
            ->Execute(
                {"XREAD",
                 "COUNT", std::to_string(config.entries_per_read.value),
                 "BLOCK", std::to_string(config.block_timeout.value),
                 "STREAMS", config.stream_key, lastId,
                 },
                [this](QVariant resp, std::exception_ptr exc){
                    try {
                        if (exc) std::rethrow_exception(exc);
                        parseReply(resp);
                    } catch (std::exception& e) {
                        Error("{}: error reading stream: {}", objectName(), e.what());
                    }
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
        RedisCmd cmd("XADD");
        cmd.Arg(config.stream_key);
        cmd.Arg("MAXLEN");
        cmd.Arg("~");
        cmd.Temp(std::to_string(config.stream_size.value));
        cmd.Arg("*");
        for (auto& [k, v]: flat) {
            cmd.Arg(k);
            cmd.Temp(v.toString().toStdString());
        }
        client->Execute(cmd, [this](QVariant, std::exception_ptr exc){
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

void radapter::builtin::workers::redis(Instance* inst) {
    inst->RegisterWorker<redis::Cache>("RedisCache", {
        {"Exec", AsExtraMethod<&redis::Cache::Exec>},
    });
    inst->RegisterSchema("RedisCache", SchemaFor<redis::CacheConfig>);

    inst->RegisterWorker<redis::Stream>("RedisStream");
    inst->RegisterSchema("RedisStream", SchemaFor<redis::StreamConfig>);
}

#include "redis.moc"
