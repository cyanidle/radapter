#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QTimer>
#include "fmt/compile.h"
#include "qtadapter.hpp"
#include "redis_client.hpp"
#include <QFile>
#include <qcoreapplication.h>
#include <set>
#include "async_helpers.hpp"

namespace radapter::redis {

enum CacheMode {
    r = 1,
    w = 2,
    rw = r | w,
};

RAD_DESCRIBE(CacheMode) {
    MEMBER("r", r);
    MEMBER("w", w);
    MEMBER("rw", rw);
    MEMBER("read", r);
    MEMBER("write", w);
    MEMBER("read_write", rw);
}

struct CacheConfig : Config {
    optional<string> hash_key;
    WithDefault<bool> enable_keyevents = true;
    WithDefault<CacheMode> mode = rw;
};

DESCRIBE("redis::CacheConfig", CacheConfig, void) {
    PARENT(Config);
    RAD_MEMBER(hash_key);
    RAD_MEMBER(enable_keyevents);
    RAD_MEMBER(mode);
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
    WithDefault<string> instance_id = "_";
    WithDefault<string> persistent_prefix = "__radapter";
    WithDefault<CacheMode> mode = rw;
};

DESCRIBE("redis::StreamConfig", StreamConfig, void) {
    PARENT(Config);
    RAD_MEMBER(stream_key);
    RAD_MEMBER(start_from);
    RAD_MEMBER(stream_size);
    RAD_MEMBER(block_timeout);
    RAD_MEMBER(entries_per_read);
    RAD_MEMBER(instance_id);
    RAD_MEMBER(persistent_prefix);
    RAD_MEMBER(mode);
}

class Cache : public Worker
{
    Q_OBJECT

    CacheConfig config;
    Client* client = nullptr;
    Client* sub_client = nullptr;
    QVariant state;
    QString preped_hash_key;
public:
    Cache(QVariantList args, Instance* inst) : Worker(inst, "redis")
    {
        Parse(config, args.value(0));
        preped_hash_key = QString::fromStdString(config.hash_key.value_or(""));
        client = new Client(config, this);
        sub_client = new Client(config, this);
        setObjectName(QString("%1_hash(%2)")
                          .arg(client->objectName())
                          .arg(config.hash_key ? config.hash_key->c_str() : "-"));
        client->setObjectName(objectName());
        sub_client->setObjectName(objectName() + "_sub");
        connect(client, &Client::Error, this, [=](QString err){
            Error("Error: {}", err);
        });
        auto onConnected = [=]{
            bool ok = client->IsConnected() && sub_client->IsConnected();
            if (ok && config.hash_key) {
                subscribeToHash();
            }
        };
        connect(client, &Client::ConnectedChanged, this, onConnected);
        client->Start();
        if (config.mode & r) {
            connect(sub_client, &Client::ConnectedChanged, this, onConnected);
            sub_client->Start();
        }
    }
    void subscribeToHash() {
        client->Execute({"CONFIG", "GET", "notify-keyspace-events"})
            .ThenSync([this](QVariant res){
                auto modes = res.toString().toStdString();
                std::set<char> sorted{modes.begin(), modes.end()};
                bool missing = sorted.find('E') == sorted.end()
                               || sorted.find('h') == sorted.end();
                if (config.enable_keyevents && missing) {
                    sorted.insert('E');
                    sorted.insert('h');
                    return client->Execute(
                        {"CONFIG", "SET",
                         "notify-keyspace-events",
                         string{sorted.begin(), sorted.end()}
                        });
                } else {
                    return fut::Resolved(QVariant{"OK"});
                }
            })
            .ThenSync([this](QVariant res){
                if (res != "OK") throw Err("Could not enable keyevent notifications");
                poll();
                sub_client->PSubscribe(fmt::format("__keyevent@{}__:*", config.db), [this](Client::SubEvent ev){
                    if (ev.message != preped_hash_key) return;
                    poll();
                });
            })
            .CatchSync([this](std::exception& e){
                Error("Could not subscribe to hash: {}", e.what());
                sub_client->ReconnectLater();
                client->ReconnectLater();
            });
    }
    void poll() {
        assert(config.hash_key);
        client->Execute({"HGETALL", *config.hash_key})
            .ThenSync([this](QVariant resp){
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
            })
            .CatchSync([this](std::exception& e) {
                Error("{}: Could not read hash {} => {}", objectName(), *config.hash_key, e.what());
            });

    }
    void OnMsg(QVariant const& msg) override {
        if (!(config.mode & w)) {
            Error("{}: write disabled", objectName());
            return;
        }
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
        client->Execute(cmd)
            .ThenSync([this](QVariant resp){
                if (resp != "OK") {
                    Error("{}: non-ok responce: {}", objectName(), resp.toString());
                }
            })
            .CatchSync([this](std::exception& e){
                Error("{}: error writing: {}", objectName(), e.what());
            });
    }

    // 1: Exec(cmd, function (ok, err) ... end) -> nil
    // 1a: Exec(cmd) -> async thunk with (ok, err)
    // 2: Exec(cmd, {arg1, arg2, ...}, function (ok, err) ... end) -> nil
    // 2a: Exec(cmd, {arg1, arg2, ...}) -> async thunk with (ok, err)
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
        auto future = client->Execute(cmd);
        if (args.size() == funcIdx) {
            //async signature
            return makeLuaPromise(this, future);
        } else {
            LuaFunction cb = args.value(funcIdx).value<LuaFunction>();
            resolveLuaCallback(this, future, cb);
            return {};
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
        client->Execute({"SET", lastIdKey, lastId})
            .CatchSync([this](std::exception& e) mutable {
                Error("{}: Could not save last id: {}", objectName(), e.what());
            });
    }
    void loadIdAndRead() {
        client->Execute({"GET", lastIdKey})
            .AtLastSync([this](Result<QVariant> res) mutable noexcept {
                try {
                    lastId = res.get().toString().toStdString();
                } catch (std::exception& e) {
                    Error("{}: Could not get last id: {}", objectName(), e.what());
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
        if (config.mode & r) {
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
            ->Execute({"XREAD", "COUNT", std::to_string(config.entries_per_read.value),
                 "BLOCK", std::to_string(config.block_timeout.value),
                 "STREAMS", config.stream_key, lastId,
            })
            .AtLastSync([this](Result<QVariant> resp){
                try {
                    parseReply(resp.get());
                    nextRead();
                } catch (std::exception& e) {
                    Error("{}: error reading stream: {}", objectName(), e.what());
                }
            });
    }
    void parseReply(QVariant resp) {
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
        if (!(config.mode & w)) {
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
        client->Execute(cmd).CatchSync([this](std::exception& e){
            Error("{}: could not write stream: {}", objectName(), e.what());
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
