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

struct CanConfig {
    QString plugin;
    QString device;
    WithDefault<std::vector<CanFilter>> filters;
    std::optional<uint64_t> bitrate;
    WithDefault<bool> can_fd = true;
    std::optional<uint64_t> data_bitrate;
};


RAD_DESCRIBE(CanConfig) {
    RAD_MEMBER(plugin);
    RAD_MEMBER(device);
    RAD_MEMBER(filters);
    RAD_MEMBER(bitrate);
    RAD_MEMBER(can_fd);
    RAD_MEMBER(data_bitrate);
}

}