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

static int payloadByte(QVariant const& value)
{
    bool ok = false;
    uint num = value.toUInt(&ok);
    if (!ok || num > 0xff) {
        Raise("CAN payload byte must be an integer in 0-255, got '{}'", value.toString().toStdString());
    }
    return int(num);
}

static QByteArray payloadBytes(QVariant const& value)
{
    if (!value.isValid()) {
        return {};
    }
    switch (value.metaType().id()) {
    case QMetaType::QVariantMap: {
        auto map = value.toMap();
        if (map.isEmpty()) {
            return {};
        }
        Raise("CAN payload must be a string, bytes, a byte value (0-255) or an array of byte values, got a map with keys [{}]",
            map.keys().join(", ").toStdString());
    }
    case QMetaType::QByteArray:
    case QMetaType::QString:
        return value.toByteArray();
    case QMetaType::QVariantList: {
        QByteArray bytes;
        for (auto const& item: value.toList()) {
            bytes.append(static_cast<char>(payloadByte(item)));
        }
        return bytes;
    }
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
        return QByteArray(1, static_cast<char>(payloadByte(value)));
    default:
        Raise("CAN payload must be a string, bytes, a byte value (0-255) or an array of byte values, got {}",
            value.metaType().name());
    }
}

class CanWorker final : public ICanWorker
{
    Q_OBJECT

    CanConfig config;
    QCanBusDevice *device = nullptr;
    qint64 pendingSinceFlush = 0;
public:
    CanWorker(CanConfig conf, radapter::Instance* inst) :
        ICanWorker(inst, EnsureName(conf, QString("%1:%2").arg(conf.plugin, conf.device)), "can")
    {
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
        device->setParent(this);
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
    void Destroy() override
    {
        if (device) {
            device->disconnectDevice();
        }
        ICanWorker::Destroy();
    }
	void OnMsg(QVariant const& _msg) override {
        QCanBusFrame frame;
        CanFrame msg;
        Parse(msg, _msg);
        if (msg.bitrate_switch && !msg.can_fd) {
            Raise("CAN bitrate_switch requires can_fd on the same frame");
        }
        if (msg.can_fd && !device->configurationParameter(QCanBusDevice::CanFdKey).toBool()) {
            Raise("CAN FD frames require the worker to be opened with can_fd = true");
        }
        frame.setExtendedFrameFormat(msg.extended_id);
        frame.setFlexibleDataRateFormat(msg.can_fd);
        frame.setBitrateSwitch(msg.bitrate_switch);
        bool ok = false;
        quint32 id = 0;
        if (msg.frame_id.metaType().id() == QMetaType::QString) {
            id = msg.frame_id.toString().toUInt(&ok, 16);
        } else {
            id = msg.frame_id.toUInt(&ok);
        }
        if (!ok) {
            Raise("frame_id should be a string (hex base16) or number");
        }
        frame.setFrameId(id);
        auto frame_type = static_cast<QCanBusFrame::FrameType>(msg.frame_type.value);
        if (frame_type != QCanBusFrame::DataFrame && frame_type != QCanBusFrame::RemoteRequestFrame) {
            QVariant name;
            Dump(msg.frame_type.value, name);
            Raise("CAN frame_type '{}' cannot be sent: only 'data' and 'request' frames are writable",
                name.toString().toStdString());
        }
        frame.setFrameType(frame_type);
        frame.setPayload(payloadBytes(msg.payload));
        if (!device->writeFrame(frame)) {
            Raise("Could not write frame: {}", device->errorString());
        }
        ++pendingSinceFlush;
        emitFlushedIfDrained();
    }
private:
    void emitFlushedIfDrained(bool force = false)
    {
        if (!pendingSinceFlush || (!force && device->framesToWrite() != 0)) {
            return;
        }
        pendingSinceFlush = 0;
        emit SendEventField("flushed", true);
    }

    void on_errorOccurred(QCanBusDevice::CanBusError err)
    {
        Error("Error: {} -> {}", QMetaEnum::fromType<QCanBusDevice::CanBusError>().valueToKey(err), device->errorString());
        emit SendEventField("error", device->errorString());
        emitFlushedIfDrained();
    }

    void on_framesReceived()
    {
        while(device->framesAvailable()) {
            QCanBusFrame frame = device->readFrame();
            emit gotFrame(frame);
            QVariantMap msg;
            msg["extended_id"] = frame.hasExtendedFrameFormat();
            msg["can_fd"] = frame.hasFlexibleDataRateFormat();
            msg["bitrate_switch"] = frame.hasBitrateSwitch();
            QVariant frame_type;
            Dump(static_cast<CanRxFrameType>(frame.frameType()), frame_type);
            msg["frame_type"] = frame_type;
            msg["frame_id"] = frame.frameId();
            msg["payload"] = frame.payload();
            emit SendMsg(msg);
        }
    }

    void on_framesWritten(qint64 framesCount)
    {
        emit SendEventField("written", framesCount);
        emitFlushedIfDrained();
    }

    void on_stateChanged(QCanBusDevice::CanBusDeviceState state)
    {
        auto state_name = QMetaEnum::fromType<QCanBusDevice::CanBusDeviceState>().valueToKey(state);
        Info("State changed: {}", state_name);
        emit SendEventField("state", state_name);
        if (state == QCanBusDevice::UnconnectedState) {
            emitFlushedIfDrained(true);
        }
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
