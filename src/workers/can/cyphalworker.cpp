#include "canframe.hpp"
#include "cyphal_helpers.h"
#include <QTimer>
#include <unordered_set>
#include "json_view/alloc.hpp"

#include "radapter_info.hpp"

#ifdef _WIN32
#pragma warning( push )
#pragma warning( disable : 4296 )
#endif

#include "uavcan/node/GetInfo_1_0.h"
#include "uavcan/node/Heartbeat_1_0.h"

#ifdef _WIN32
#pragma warning( pop )
#endif

#include "radapter/async_helpers.hpp"

using namespace std::chrono_literals;

namespace radapter::can
{

struct CyphalTopic
{
    QString type;
    CanardPortID port;
};

RAD_DESCRIBE(CyphalTopic)
{
    RAD_MEMBER(type);
    RAD_MEMBER(port);
}

struct LocalCyphalService : CyphalTopic
{
    LuaFunction handler;
};

RAD_DESCRIBE(LocalCyphalService)
{
    PARENT(CyphalTopic);
    RAD_MEMBER(handler);
}

struct CyphalNodeInfoVersion
{
    uint8_t major;
    uint8_t minor;
};

RAD_DESCRIBE(CyphalNodeInfoVersion)
{
    RAD_MEMBER(major);
    RAD_MEMBER(minor);
}

struct CyphalNodeInfo
{
    WithDefault<CyphalNodeInfoVersion> protocol_version = CyphalNodeInfoVersion{1, 0};
    WithDefault<CyphalNodeInfoVersion> hardware_version = CyphalNodeInfoVersion{VerMajor, VerMinor};
    WithDefault<CyphalNodeInfoVersion> software_version = CyphalNodeInfoVersion{1, 0};
    WithDefault<uint64_t> software_vcs_revision_id = QString(BuildId).toULongLong(nullptr, 16);
    WithDefault<QUuid> unique_id = QUuid::createUuid();
    WithDefault<QString> name = QString("org.radapter");
    WithDefault<std::vector<uint64_t>> software_image_crc;
    WithDefault<QString> certificate_of_authenticity;
};

RAD_DESCRIBE(CyphalNodeInfo)
{
    RAD_MEMBER(protocol_version);
    RAD_MEMBER(hardware_version);
    RAD_MEMBER(software_version);
    RAD_MEMBER(software_vcs_revision_id);
    RAD_MEMBER(unique_id);
    RAD_MEMBER(name);
    RAD_MEMBER(software_image_crc);
    RAD_MEMBER(certificate_of_authenticity);
}

struct CyphalConfig : WorkerConfig
{
    QObject* can;
    CanardNodeID node_id;
    WithDefault<int> heartbeat_period = 1000;
    WithDefault<size_t> tx_cap = 100ull;
    WithDefault<std::map<QString, CyphalTopic>> subscribe;
    WithDefault<std::map<QString, CyphalTopic>> publish;
    WithDefault<CyphalNodeInfo> node_info;
    WithDefault<std::vector<LocalCyphalService>> services;
};

RAD_DESCRIBE(CyphalConfig)
{
    PARENT(WorkerConfig);
    RAD_MEMBER(can);
    RAD_MEMBER(node_id);
    RAD_MEMBER(heartbeat_period);
    RAD_MEMBER(tx_cap);
    RAD_MEMBER(subscribe);
    RAD_MEMBER(publish);
    RAD_MEMBER(node_info);
    RAD_MEMBER(services);
}

struct RequestParams
{
    QString type;
    CanardNodeID server;
    CanardPortID port;
    WithDefault<uint32_t> timeout = 5000u;
};

RAD_DESCRIBE(RequestParams)
{
    RAD_MEMBER(type);
    RAD_MEMBER(server);
    RAD_MEMBER(port);
    RAD_MEMBER(timeout);
}

class CyphalWorker final: public Worker
{
    Q_OBJECT
private:
    CyphalConfig config;
    CanardInstance canard;
    CanardTxQueue tx;
    QPointer<ICanWorker> can;
    QTime start;
    std::unordered_map<CanardPortID, CanardTransferID> pub_tids;

    struct RxSub : CanardRxSubscription
    {
        const CanardMessageDynamic* dyn;
        QString name;
    };

    struct Service : CanardRxSubscription {
        QString type_name;
        const CanardMessageDynamic* req_dyn;
        const CanardMessageDynamic* resp_dyn;
        MoveFunc<auto(QVariant) -> Future<QVariant>> handler;
    };

    struct RequestState {
        fut::Promise<QVariant> promise;
        std::chrono::milliseconds timeout;
    };

    struct RequestsPort : CanardRxSubscription {
        const CanardMessageDynamic* resp_dyn;
        std::unordered_map<CanardTransferID, RequestState> in_flight;
        CanardTransferID tid = 0;
    };

    std::unordered_map<CanardPortID, RequestsPort> reqs;

    std::list<RxSub> subs_storage;
    std::list<Service> srvs_storage;
    std::vector<uint8_t> tx_buffer;

    struct PubMeta {
        const CanardMessageDynamic* dyn;
        CanardPortID port;
    };
    std::unordered_map<QString, PubMeta> pubs;
    QVariant node_info_resp;
public:
    CyphalWorker(CyphalConfig conf, radapter::Instance* inst) :
        radapter::Worker(inst, EnsureName(conf, QString("cyphal:%1").arg(conf.node_id)), "cyphal")
    {
        config = std::move(conf);
        if (conf.node_id > CANARD_NODE_ID_MAX)
            Raise("Node id is too big: {} (max: {})", conf.node_id, CANARD_NODE_ID_MAX);
        can = qobject_cast<ICanWorker*>(config.can);
        canard = canardInit(CANARD_WRAP(memAllocate), CANARD_WRAP(memFree));
        canard.user_reference = this;
        canard.node_id = config.node_id;
        bool is_fd = can->get_device()->configurationParameter(QCanBusDevice::CanFdKey).toBool();
        tx = canardTxInit(config.tx_cap, is_fd ? CANARD_MTU_CAN_FD : CANARD_MTU_CAN_CLASSIC);
        QTimer* hb = new QTimer(this);
        hb->callOnTimeout(this, &CyphalWorker::heartbeat);
        hb->start(config.heartbeat_period);
        start = QTime::currentTime();
        connect(can, &ICanWorker::gotFrame, this, &CyphalWorker::on_frame);
        for (auto& [name, topic]: config.publish.value) {
            const CanardMessageDynamic* dyn = lookup_canard_type(topic.type);
            if (!dyn) {
                Raise("Pub {}: Canard type '{}' not recognized", name, topic.type);
            }
            pubs[name] = PubMeta{dyn, topic.port};
        }

        auto filterList = can->get_device()->configurationParameter(QCanBusDevice::RawFilterKey).value<QList<QCanBusDevice::Filter>>();
        auto add_filter = [&](CanardFilter current) {
            QCanBusDevice::Filter filter;
            filter.frameId = current.extended_can_id;
            filter.frameIdMask = current.extended_mask;
            filter.format = QCanBusDevice::Filter::MatchExtendedFormat;
            filter.type = QCanBusFrame::DataFrame;
            filterList.append(filter);
        };
        for (auto& [name, topic]: config.subscribe.value) {
            const CanardMessageDynamic* dyn = lookup_canard_type(topic.type);
            if (!dyn) {
                Raise("Sub {}: Canard type '{}' not recognized", name, topic.type);
            }
            RxSub* sub = &subs_storage.emplace_back();
            sub->dyn = dyn;
            sub->name = name;
            CanardPortID port_id = topic.port;
            if (!canardRxSubscribe(&canard, CanardTransferKindMessage, port_id, dyn->extent, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, sub)) {
                Raise("Could not subscribe to msgs of type({}) on port:{}", topic.type, port_id);
            }
            add_filter(canardMakeFilterForSubject(port_id));
        }
        if (TagsEnabled()) {
            QStringList fields;
            for (auto& [name, topic]: config.subscribe.value)
                fields << name;
            AdvertiseFields(fields);
        }
        for (auto& service: config.services.value) {
            auto [req, resp] = lookup_service_types(service.type);
            if (!req || !resp) {
                Raise("Service: Canard service type '{}' not recognized", service.type);
            }
            Service* srv = &srvs_storage.emplace_back();
            srv->type_name = service.type;
            srv->req_dyn = req;
            srv->resp_dyn = resp;
            srv->handler = [func = service.handler](QVariant msg){
                return func.CallAsync(QVariantList{msg});
            };
            CanardPortID port_id = service.port;
            if (!canardRxSubscribe(&canard, CanardTransferKindRequest, port_id, req->extent, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, srv)) {
                Raise("Could not subscribe to msgs of type({}) on port:{}", service.type, port_id);
            }
        }
        { // NodeInfo handler
            Dump(config.node_info, node_info_resp);
            auto* info_sub = &subs_storage.emplace_back();
            info_sub->user_reference = (void*)lookup_canard_type(u"uavcan.node.GetInfo.Response.1.0");
            assert(info_sub->user_reference);
            canardRxSubscribe(&canard, CanardTransferKindRequest, uavcan_node_GetInfo_1_0_FIXED_PORT_ID_, uavcan_node_GetInfo_Request_1_0_EXTENT_BYTES_, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, info_sub);
        }
        add_filter(canardMakeFilterForServices(config.node_id));
        can->get_device()->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(filterList));

        auto checker = new QTimer(this);
        checker->callOnTimeout(this, &CyphalWorker::checkReqTimeouts);
        checker->start(500ms);
    }

    void checkReqTimeouts() {
        for (auto port = reqs.begin(); port != reqs.end();)
        {
            auto& in_flight = port->second.in_flight;
            for (auto req = in_flight.begin(); req != in_flight.end();) {
                if (req->second.timeout < 500ms) {
                    req = in_flight.erase(req);
                } else {
                    req->second.timeout -= 500ms;
                    ++req;
                }
            }
            if (port->second.in_flight.empty()) {
                canardRxUnsubscribe(&canard, CanardTransferKindResponse, port->second.port_id);
                port = reqs.erase(port);
            } else {
                ++port;
            }
        }
    }
    void pushMsg(const CanardTransferMetadata* meta, const CanardMessageDynamic* dyn, QVariant const& msg) {
        jv::DefaultArena<> arena;
        size_t buf_size = dyn->extent;
        auto* buf = static_cast<uint8_t*>(arena.Allocate(buf_size, 1));
        dyn->serialize(msg, buf, buf_size);
        auto err = canardTxPush(&tx, &canard, 0, meta, buf_size, buf);
        if (err == -CANARD_ERROR_INVALID_ARGUMENT) {
            Raise("Could not push msg of type: {}: Invalid Argument", QString(dyn->name_and_ver.data(), int(dyn->name_and_ver.size())));
        }
        if (err == -CANARD_ERROR_OUT_OF_MEMORY) {
            Raise("Could not push msg of type: {}: Out of memory", QString(dyn->name_and_ver.data(), int(dyn->name_and_ver.size())));
        }
    }
    QVariant Request(RequestParams params, QVariant msg, std::optional<LuaFunction> cb) {
        auto [req, resp] = lookup_service_types(params.type);
        if (!req || !resp)
            Raise("Could not find types for service: {}", params.type);

        RequestsPort& port = reqs[params.port];
        auto tid = port.tid++;

        if (!port.resp_dyn) {
            port.resp_dyn = resp;
        } else if (port.resp_dyn != resp) {
            Raise("Cannot have conflicting types for requests on port: {} (was: {}, passed: {})",
                  port.port_id,
                  port.resp_dyn->name_and_ver.toString(),
                  resp->name_and_ver.toString());
        }

        if (port.in_flight.find(tid) != port.in_flight.end())
            Raise("Too many requests on port: {}", params.port);

        auto& state = port.in_flight[tid];

        CanardTransferMetadata meta;
        meta.transfer_id = tid;
        meta.port_id = params.port;
        meta.priority = CanardPriorityNominal;
        meta.remote_node_id = params.server;
        meta.transfer_kind = CanardTransferKindRequest;

        if (port.in_flight.size() == 1) {
            canardRxSubscribe(&canard, CanardTransferKindResponse, params.port, req->extent, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, &port);
        }

        fut::Future<QVariant> future = state.promise.GetFuture();
        state.timeout = std::chrono::milliseconds{params.timeout.value};

        pushMsg(&meta, req, msg);
        processTx();

        if (cb) {
            resolveLuaCallback(this, future, *cb);
            return {};
        } else {
            return makeLuaPromise(this, future);
        }
    }
    void OnMsg(QVariant const& msg) override {
        auto map = msg.toMap();
        for (auto it = map.constKeyValueBegin(); it != map.constKeyValueEnd(); ++it) {
            const auto& [k, v] = *it;
            auto pit = pubs.find(k);
            if (pit == pubs.end()) {
                Warn("Publisher topic with name {} not registered", k);
                continue;
            }
            auto [dyn, port] = pit->second;
            const CanardTransferMetadata transfer_metadata  = {
                CanardPriorityNominal,
                CanardTransferKindMessage,
                port,
                CANARD_NODE_ID_UNSET,
                pub_tids[port]++,
            };
            pushMsg(&transfer_metadata, dyn, v);
        }
        processTx();
    }
private:
    void heartbeat() {
        auto now = QTime::currentTime();
        uavcan_node_Heartbeat_1_0 test_heartbeat = {};
        test_heartbeat.uptime = static_cast<uint32_t>(start.secsTo(now));
        test_heartbeat.health = {uavcan_node_Health_1_0_NOMINAL};
        test_heartbeat.mode = {uavcan_node_Mode_1_0_OPERATIONAL};
        const CanardTransferMetadata transfer_metadata  = {
			CanardPriorityNominal,
			CanardTransferKindMessage,
			uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_,
			CANARD_NODE_ID_UNSET,
            pub_tids[uavcan_node_Heartbeat_1_0_FIXED_PORT_ID_]++,
		};
        size_t hbeat_ser_buf_size = uavcan_node_Heartbeat_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_;
        uint8_t hbeat_ser_buf[uavcan_node_Heartbeat_1_0_SERIALIZATION_BUFFER_SIZE_BYTES_];
        uavcan_node_Heartbeat_1_0_serialize_(&test_heartbeat, hbeat_ser_buf, &hbeat_ser_buf_size);
        canardTxPush(&tx, &canard, 0, &transfer_metadata, hbeat_ser_buf_size, hbeat_ser_buf);
        processTx();
    }
    static uint64_t micros() {
        auto ts = std::chrono::high_resolution_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::duration<uint64_t, std::micro>>(ts).count();
    }
    void processTx() {
        auto* ican = can.data();
        if (!ican) {
            Error("Could not send frame: can is dead");
        }
        for (const CanardTxQueueItem* ti = NULL; (ti = canardTxPeek(&tx)) != NULL;)
        {
            if ((0U == ti->tx_deadline_usec) || (ti->tx_deadline_usec > micros()))  // Check the deadline.
            {
                QCanBusFrame frame;
                frame.setExtendedFrameFormat(true);
                frame.setFrameId(ti->frame.extended_can_id);
                frame.setPayload(QByteArray::fromRawData(reinterpret_cast<const char*>(ti->frame.payload), static_cast<int>(ti->frame.payload_size)));
                can->get_device()->writeFrame(frame);
            }
            canard.memory_free(&canard, canardTxPop(&tx, ti));
        }
    }
    void on_frame(QCanBusFrame const& frame) try {
        CanardFrame rxf;
        rxf.extended_can_id = frame.frameId();
        auto payload = frame.payload();
        rxf.payload = payload.data();
        rxf.payload_size = size_t(payload.size());
	    CanardRxTransfer transfer;
        CanardRxSubscription* sub;
        if (!canardRxAccept(&canard, micros(), &rxf, 0, &transfer, reinterpret_cast<CanardRxSubscription**>(&sub))) {
            return;
        }
        if (transfer.metadata.port_id == uavcan_node_GetInfo_1_0_FIXED_PORT_ID_) {
            auto* resp_dyn = static_cast<const CanardMessageDynamic*>(sub->user_reference);
            auto meta = transfer.metadata;
            meta.transfer_kind = CanardTransferKindResponse;
            pushMsg(&meta, resp_dyn, node_info_resp);
            processTx();
        } else if (transfer.metadata.transfer_kind == CanardTransferKindResponse) {
            auto& meta = transfer.metadata;
            RequestsPort* req_port = static_cast<RequestsPort*>(sub);
            auto curr = req_port->in_flight.find(meta.transfer_id);
            if (curr == req_port->in_flight.end()) {
                Error("Could not find response handler for server:{} port:{} transfer_id:{}",
                      meta.remote_node_id, meta.port_id, meta.transfer_id);
            }
            auto& handler = curr->second;
            auto res = req_port->resp_dyn->deserialize(reinterpret_cast<const uint8_t*>(transfer.payload), transfer.payload_size);
            handler.promise(std::move(res));
            req_port->in_flight.erase(curr);
            if (req_port->in_flight.empty())
                canardRxUnsubscribe(&canard, CanardTransferKindResponse, req_port->port_id);
        }  else if (transfer.metadata.transfer_kind == CanardTransferKindRequest) {
            auto* service = static_cast<Service*>(sub);
            auto meta = transfer.metadata;
            QVariant req = service->req_dyn->deserialize(reinterpret_cast<const uint8_t*>(transfer.payload), transfer.payload_size);
            canard.memory_free(&canard, transfer.payload);
            service->handler(req).AtLastSync([ref = QPointer(this), service, meta](fut::Result<QVariant> res){
                auto self = ref.data();
                if (!self)
                    return;
                self->handleLocalResp(meta, service, res);
            });
        } else {
            auto* rx_sub = static_cast<RxSub*>(sub);
            QVariant msg = rx_sub->dyn->deserialize(reinterpret_cast<const uint8_t*>(transfer.payload), transfer.payload_size);
            canard.memory_free(&canard, transfer.payload);
            emit SendMsgField(rx_sub->name, msg);
        }
    } catch (std::exception& e) {
        Error("Could not receive frame: {}", e.what());
    }

    void handleLocalResp(CanardTransferMetadata meta, Service* srv, fut::Result<QVariant> resp) {
        if (!resp) {
            try {
                std::rethrow_exception(resp.get_exception());
            } catch (std::exception& e) {
                Error("Error in service: {} on port: {}. {}", srv->type_name, srv->port_id, e.what());
            }
            return;
        }
        try {
            meta.transfer_kind = CanardTransferKindResponse;
            pushMsg(&meta, srv->resp_dyn, resp.get());
            processTx();
        } catch (std::exception& e) {
            Error("Invalid response for: {} on port: {}. {}", srv->type_name, srv->port_id, e.what());
        }
    }

    void *memAllocate(const size_t amount) {
        return malloc(amount);
    }

    void memFree(void *const pointer) {
        free(pointer);
    }
};

}

namespace radapter::builtin::workers
{

void cyphal(Instance* inst)
{
    inst->RegisterSchema<can::CyphalConfig>("Cyphal");
    inst->RegisterWorker<can::CyphalWorker>("Cyphal", ExtraMethods{
        {"Request", AsExtraMethod<&can::CyphalWorker::Request>}
    });
}

}

#include "cyphalworker.moc"
