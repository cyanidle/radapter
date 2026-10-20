#pragma once

#include <builtin.hpp>
#include <QtSerialBus/QCanBusDevice>

class QCanBusDevice;

namespace radapter::can
{

class CanWorker;

class ICanWorker : public Worker
{
    Q_OBJECT
public:
    using Worker::Worker;

    virtual QCanBusDevice* get_device() = 0;
signals:
    void gotFrame(QCanBusFrame const& frane);
};

enum class CanFrameType
{
    data = QCanBusFrame::DataFrame,
    request = QCanBusFrame::RemoteRequestFrame,
    error = QCanBusFrame::ErrorFrame,
};

RAD_DESCRIBE(CanFrameType) {
    RAD_ENUM(data);
    RAD_ENUM(request);
    RAD_ENUM(error);
}

enum class CanRxFrameType
{
    data = QCanBusFrame::DataFrame,
    request = QCanBusFrame::RemoteRequestFrame,
    error = QCanBusFrame::ErrorFrame,
    unknown = QCanBusFrame::UnknownFrame,
    invalid = QCanBusFrame::InvalidFrame,
};

RAD_DESCRIBE(CanRxFrameType) {
    RAD_ENUM(data);
    RAD_ENUM(request);
    RAD_ENUM(error);
    RAD_ENUM(unknown);
    RAD_ENUM(invalid);
}

struct CanFrame {
    QVariant frame_id;
    QVariant payload;
    WithDefault<bool> extended_id = false;
    WithDefault<bool> can_fd = false;
    WithDefault<bool> bitrate_switch = false;
    WithDefault<CanRxFrameType> frame_type = CanRxFrameType::data;
};

RAD_DESCRIBE(CanFrame) {
    RAD_MEMBER(frame_id);
    RAD_MEMBER(payload);
    RAD_MEMBER(extended_id);
    RAD_MEMBER(can_fd);
    RAD_MEMBER(bitrate_switch);
    RAD_MEMBER(frame_type);
}

enum class CanFilterMatch
{
    normal = QCanBusDevice::Filter::MatchBaseFormat,
    extended = QCanBusDevice::Filter::MatchExtendedFormat,
    both = QCanBusDevice::Filter::MatchBaseAndExtendedFormat,
};

RAD_DESCRIBE(CanFilterMatch) {
    RAD_ENUM(both);
    RAD_ENUM(normal);
    RAD_ENUM(extended);
}

struct CanFilter {
    WithDefault<CanFilterMatch> match = CanFilterMatch::both;
    WithDefault<CanFrameType> type = CanFrameType::data;
    qlonglong id;
    qlonglong mask;
};

RAD_DESCRIBE(CanFilter) {
    RAD_MEMBER(match);
    RAD_MEMBER(type);
    RAD_MEMBER(id);
    RAD_MEMBER(mask);
}

struct CanConfig : WorkerConfig {
    QString plugin;
    QString device;
    WithDefault<std::vector<CanFilter>> filters;
    std::optional<uint64_t> bitrate;
    WithDefault<bool> can_fd = false;
    std::optional<uint64_t> data_bitrate;
};


RAD_DESCRIBE(CanConfig) {
    PARENT(WorkerConfig);
    RAD_MEMBER(plugin);
    RAD_MEMBER(device);
    RAD_MEMBER(filters);
    RAD_MEMBER(bitrate);
    RAD_MEMBER(can_fd);
    RAD_MEMBER(data_bitrate);
}

}
