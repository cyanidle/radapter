#include <qmetaobject.h>
#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include <builtin.hpp>
#include <QtSerialBus/QCanBus>
#include <QtSerialBus/QCanBusDevice>
#include <QtSerialBus/QCanBusFactory>

#include "canframe.hpp"

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

class CanWorker final : public ICanWorker
{
    Q_OBJECT

    CanConfig config;
    QCanBusDevice *device = nullptr;
public:
    CanWorker(CanConfig conf, radapter::Instance* inst) : ICanWorker(inst, "can") {
        config = std::move(conf);
        QString errorString;
        QCanBus* bus = QCanBus::instance();
        device = bus->createDevice(config.plugin, config.device, &errorString);
        if (!device) {
            Raise("Could not create CAN worker: {}.\nAvailable plugins: [{}].\nDevices for current plugin({}): [{}]",
                errorString,
                fmt::join(bus->plugins(), ", "),
                config.plugin,
                fmt::join(bus->availableDevices(config.plugin), ", "));
        }
        connect(device, &QCanBusDevice::framesReceived, this, &CanWorker::on_framesReceived);
        connect(device, &QCanBusDevice::errorOccurred, this, &CanWorker::on_errorOccurred);
        connect(device, &QCanBusDevice::framesWritten, this, &CanWorker::on_framesWritten);
        connect(device, &QCanBusDevice::stateChanged, this, &CanWorker::on_stateChanged);
        QList<QCanBusDevice::Filter> filterList;
        for (const auto& filter: config.filters.value) {
            QCanBusDevice::Filter f;
            f.format = static_cast<QCanBusDevice::Filter::FormatFilter>(filter.match.value);
            f.type = static_cast<QCanBusFrame::FrameType>(filter.type.value);
            f.frameId = static_cast<quint32>(filter.id);
            f.frameIdMask = static_cast<quint32>(filter.mask);
            filterList.append(f);
        }
        device->setConfigurationParameter(QCanBusDevice::CanFdKey, config.can_fd.value);
        if (config.bitrate) {
            device->setConfigurationParameter(QCanBusDevice::BitRateKey, QVariant::fromValue(*config.bitrate));
        }
        if (config.data_bitrate) {
            device->setConfigurationParameter(QCanBusDevice::DataBitRateKey, QVariant::fromValue(*config.data_bitrate));
        }
        if (filterList.size()) {
            device->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(filterList));
        }
        if (!device->connectDevice()) {
            Raise("Could not connect CAN device ({}:{}): {}", config.plugin, config.device, device->errorString());
        }
    }
    QCanBusDevice* get_device() override
    {
        return device;
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
            Raise("frame_id should be a string (hex base16) or number");
        }
        frame.setFrameId(id);
        frame.setFrameType(QCanBusFrame::DataFrame);
        frame.setPayload(msg.payload.value<QByteArray>());
        if (!device->writeFrame(frame)) {
            Raise("Could not write frame: {}", device->errorString());
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
            emit gotFrame(frame);
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
