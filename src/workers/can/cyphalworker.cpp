#include "canframe.hpp"
#include "cyphal_helpers.h"
#include <QTimer>
#include "json_view/alloc.hpp"
#include "uavcan/node/Heartbeat_1_0.h"

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

struct CyphalConfig
{
    QObject* can;
    CanardNodeID node_id;
    WithDefault<int> heartbeat_period = 1000;
    WithDefault<size_t> tx_cap = 100ull;
    WithDefault<std::map<QString, CyphalTopic>> subscribe;
    WithDefault<std::map<QString, CyphalTopic>> publish;
};

RAD_DESCRIBE(CyphalConfig)
{
    RAD_MEMBER(can);
    RAD_MEMBER(node_id);
    RAD_MEMBER(tx_cap);
    RAD_MEMBER(heartbeat_period);
    RAD_MEMBER(subscribe);
    RAD_MEMBER(publish);
}

class CyphalWorker final: public Worker
{
    Q_OBJECT
private:
    CyphalConfig config;
    CanardInstance canard;
    CanardTxQueue tx;
    QSharedPointer<QObject> can;
    ICanWorker* ican;
    QTime start;
    CanardTransferID tid = {};
    std::list<CanardRxSubscription> subs_storage;
    std::vector<uint8_t> tx_buffer;

    struct SubMeta {
        const CanardMessageDynamic* dyn;
        QString name;
    };
    std::vector<SubMeta> sub_metas;

    struct PubMeta {
        const CanardMessageDynamic* dyn;
        CanardPortID port;
    };
    std::unordered_map<QString, PubMeta> pubs;
    jv::DefaultArena<> send_arena;
public:
    CyphalWorker(CyphalConfig conf, radapter::Instance* inst) : radapter::Worker(inst, "cyphal") {
        config = std::move(conf);
        can.reset(config.can);
        canard = canardInit(CANARD_WRAP(memAllocate), CANARD_WRAP(memFree));
        canard.user_reference = this;
        canard.node_id = config.node_id;
        ican = qobject_cast<ICanWorker*>(can.get());
        bool is_fd = ican->get_device()->configurationParameter(QCanBusDevice::CanFdKey).toBool();
        tx = canardTxInit(config.tx_cap, is_fd ? CANARD_MTU_CAN_FD : CANARD_MTU_CAN_CLASSIC);
        QTimer* hb = new QTimer(this);
        hb->callOnTimeout(this, &CyphalWorker::heartbeat);
        hb->start(config.heartbeat_period);
        start = QTime::currentTime();
        connect(ican, &ICanWorker::gotFrame, this, &CyphalWorker::on_frame);
        auto filterList = ican->get_device()->configurationParameter(QCanBusDevice::RawFilterKey).value<QList<QCanBusDevice::Filter>>();
        for (auto& [name, topic]: config.publish.value) {
            const CanardMessageDynamic* dyn = lookup_canard_type(topic.type);
            if (!dyn) {
                Raise("Pub {}: Canard type '{}' not recognized", name, topic.type);
            }
            pubs[name] = PubMeta{dyn, topic.port};
        }
        for (auto& [name, topic]: config.subscribe.value) {
            const CanardMessageDynamic* dyn = lookup_canard_type(topic.type);
            if (!dyn) {
                Raise("Sub {}: Canard type '{}' not recognized", name, topic.type);
            }
            CanardRxSubscription* sub = &subs_storage.emplace_back();
            SubMeta& meta = sub_metas.emplace_back();
            meta.dyn = dyn;
            meta.name = name;
            sub->user_reference = reinterpret_cast<void*>(sub_metas.size() - 1);
            CanardPortID port_id = topic.port;
            if (!canardRxSubscribe(&canard, CanardTransferKindMessage, port_id, dyn->extent, CANARD_DEFAULT_TRANSFER_ID_TIMEOUT_USEC, sub)) {
                Raise("Could not subscribe to msgs of type({}) on port:{}", topic.type, port_id);
            }
            CanardFilter current = canardMakeFilterForSubject(port_id);
            QCanBusDevice::Filter filter;
            filter.frameId = current.extended_can_id;
            filter.frameIdMask = current.extended_mask;
            filter.format = QCanBusDevice::Filter::MatchExtendedFormat;
            filter.type = QCanBusFrame::DataFrame;
            filterList.append(filter);
        }
        ican->get_device()->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(filterList));
    }
    void pushMsg(const CanardTransferMetadata* meta, const CanardMessageDynamic* dyn, QVariant const& msg) {
        size_t buf_size = dyn->extent;
        auto* buf = static_cast<uint8_t*>(send_arena.Allocate(buf_size, 1));
        dyn->serialize(msg, buf, buf_size);
        auto err = canardTxPush(&tx, &canard, 0, meta, buf_size, buf);
        if (err == CANARD_ERROR_INVALID_ARGUMENT) {
            Raise("Could not push msg of type: {}: Invalid Argument", dyn->name_and_ver);
        }
        if (err == CANARD_ERROR_OUT_OF_MEMORY) {
            Raise("Could not push msg of type: {}: Out of memory", dyn->name_and_ver);
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
                tid++,
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
			tid++,
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
        for (const CanardTxQueueItem* ti = NULL; (ti = canardTxPeek(&tx)) != NULL;)
        {
            if ((0U == ti->tx_deadline_usec) || (ti->tx_deadline_usec > micros()))  // Check the deadline.
            {
                QCanBusFrame frame;
                frame.setExtendedFrameFormat(true);
                frame.setFrameId(ti->frame.extended_can_id);
                frame.setPayload(QByteArray::fromRawData(reinterpret_cast<const char*>(ti->frame.payload), static_cast<int>(ti->frame.payload_size)));
                ican->get_device()->writeFrame(frame);
            }
            canard.memory_free(&canard, canardTxPop(&tx, ti));
        }
        send_arena.Clear();
    }
    void on_frame(QCanBusFrame const& frame) {
        CanardFrame rxf;
        rxf.extended_can_id = frame.frameId();
        auto payload = frame.payload();
        rxf.payload = payload.data();
        rxf.payload_size = size_t(payload.size());
	    CanardRxTransfer transfer;
        CanardRxSubscription* sub;
        if (!canardRxAccept(&canard, micros(), &rxf, 0, &transfer, &sub)) {
            return;
        }
        SubMeta& meta = sub_metas[reinterpret_cast<size_t>(sub->user_reference)];
        QVariant msg = meta.dyn->deserialize(reinterpret_cast<const uint8_t*>(transfer.payload), transfer.payload_size);
        canard.memory_free(&canard, transfer.payload);
        emit SendMsgField(meta.name, msg);
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
    inst->RegisterWorker<can::CyphalWorker>("Cyphal");
}

}

#include "cyphalworker.moc"
