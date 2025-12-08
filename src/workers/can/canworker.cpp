#include <qmetaobject.h>
#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include <builtin.hpp>
#include <QtSerialBus/QCanBus>
#include <QtSerialBus/QCanBusDevice>
#include <QtSerialBus/QCanBusFactory>

template<>
struct fmt::formatter<QCanBusDeviceInfo> : fmt::formatter<QString>
{
    template<typename Ctx>
    auto format(QCanBusDeviceInfo const& val, Ctx& ctx) const {
        return fmt::formatter<QString>::format(val.name(), ctx);
    }
};

namespace radapter::can
{

enum class CanFilterMatch
{
    NormalID = 1,
    ExtendedID = 2,
    Both = 2,
};

struct CanFilter {

};

RAD_DESCRIBE(CanFilter) {
}

struct CanConfig {
    QString plugin;
    QString device;
    WithDefault<std::vector<CanFilter>> filters;
    std::optional<uint64_t> baudrate;
};

RAD_DESCRIBE(CanConfig) {
    RAD_MEMBER(plugin);
    RAD_MEMBER(device);
    RAD_MEMBER(filters);
    RAD_MEMBER(baudrate);
}

struct CanFrame {
    QVariant frame_id;
    QVariant payload;
    WithDefault<bool> extended_id = false;
    WithDefault<bool> can_fd = false;
};

RAD_DESCRIBE(CanFrame) {
    RAD_MEMBER(frame_id);
    RAD_MEMBER(payload);
    RAD_MEMBER(extended_id);
    RAD_MEMBER(can_fd);
}

class CanWorker final : public radapter::Worker
{
    Q_OBJECT

    CanConfig config;
    QCanBusDevice *device = nullptr;
public:
    CanWorker(QVariantList const& args, radapter::Instance* inst) : radapter::Worker(inst, "can") {
        Parse(config, args.at(0));
        QString errorString;
        QCanBus* bus = QCanBus::instance();
        device = bus->createDevice(config.plugin, config.device, &errorString);
        if (!device) {
            throw Err("Could not create CAN worker: {}.\nAvailable plugins: [{}].\nDevices for current plugin({}): [{}]",
                errorString,
                fmt::join(bus->plugins(), ", "),
                config.plugin,
                fmt::join(bus->availableDevices(config.plugin), ", "));
        }
        connect(device, &QCanBusDevice::framesReceived, this, &CanWorker::on_framesReceived);
        connect(device, &QCanBusDevice::errorOccurred, this, &CanWorker::on_errorOccurred);
        connect(device, &QCanBusDevice::framesWritten, this, &CanWorker::on_framesWritten);
        connect(device, &QCanBusDevice::stateChanged, this, &CanWorker::on_stateChanged);
        for (const auto& filter: config.filters.value) {
            QCanBusDevice::Filter f;
            //todo
        }
        if (!device->connectDevice()) {
            throw Err("Could not connect CAN device ({}:{}): {}", config.plugin, config.device, device->errorString());
        }
    }
	void OnMsg(QVariant const& _msg) override {
        QCanBusFrame frame;
        CanFrame msg;
        Parse(msg, _msg);
        frame.setExtendedFrameFormat(msg.extended_id);
        frame.setFlexibleDataRateFormat(msg.can_fd);
        frame.setBitrateSwitch(msg.can_fd);
        auto str = msg.frame_id.value<QString>();
        quint32 id;
        bool ok = false;
        if (!str.isEmpty()) {
            id = str.toUInt(&ok);
        } else {
            bool ok = false;
            id = msg.frame_id.toUInt(&ok);
        }
        if (!ok) {
            throw Err("frame_id should be a string (hex base16) or number");
        }
        frame.setFrameId(id);
        frame.setFrameType(QCanBusFrame::DataFrame);
        frame.setPayload(msg.payload.value<QByteArray>());
        if (!device->writeFrame(frame)) {
            throw Err("Could not write frame: {}", device->errorString());
        }
    }
private:
    void on_errorOccurred(QCanBusDevice::CanBusError err)
    {
        Error("Error: {} -> {}", QMetaEnum::fromType<QCanBusDevice::CanBusError>().valueToKey(err), device->errorString());
        emit SendEventField("error", device->errorString());
    }

    void on_framesReceived()
    {
        while(device->framesAvailable()) {
            QCanBusFrame frame = device->readFrame();
            QVariantMap msg;
            msg["extended_id"] = frame.hasExtendedFrameFormat();
            msg["can_fd"] = frame.hasFlexibleDataRateFormat();
            msg["frame_id"] = frame.frameId();
            msg["payload"] = frame.payload();
            emit SendMsg(msg);
        }
    }

    void on_framesWritten(qint64 framesCount)
    {
        emit SendEventField("written", framesCount);
    }

    void on_stateChanged(QCanBusDevice::CanBusDeviceState state)
    {
        auto state_name = QMetaEnum::fromType<QCanBusDevice::CanBusDeviceState>().valueToKey(state);
        Info("State changed: {}", state_name);
        emit SendEventField("state", state_name);
    }
};

}

namespace radapter::builtin {


void workers::can(Instance* inst) 
{
    inst->RegisterWorker<can::CanWorker>("CAN");
	inst->RegisterSchema<can::CanConfig>("CAN");
}

}


#include "canworker.moc"