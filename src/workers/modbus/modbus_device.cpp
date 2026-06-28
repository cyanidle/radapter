#include "modbus_device.hpp"
#include <QModbusRtuSerialServer>
#include <qmodbustcpserver.h>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

radapter::modbus::MasterDevice::MasterDevice(RtuDevice config, QObject *parent) :
    MasterDevice(static_cast<Device&>(config), parent)
{
    using Param = QModbusRtuSerialMaster::ConnectionParameter;
    auto dev = new QModbusRtuSerialMaster(this);
    dev->setInterFrameDelay(int(config.frame_gap.value));
    device = dev;
    connectionString = config.port;
    device->setConnectionParameter(Param::SerialBaudRateParameter, config.baud.value);
    device->setConnectionParameter(Param::SerialDataBitsParameter, config.data_bits.value);
    device->setConnectionParameter(Param::SerialStopBitsParameter, config.stop_bits.value);
    device->setConnectionParameter(Param::SerialParityParameter, config.parity.value);
    device->setConnectionParameter(Param::SerialPortNameParameter, QString::fromStdString(config.port));
    setObjectName(QString::fromStdString(fmt::format("Device({}/{})", connectionString, config.name.value)));
}

radapter::modbus::MasterDevice::MasterDevice(TcpDevice config, QObject *parent) :
    MasterDevice(static_cast<Device&>(config), parent)
{
    using Param = QModbusTcpClient::ConnectionParameter;
    auto dev = new QModbusTcpClient(this);
    device = dev;
    connectionString = fmt::format("{}:{}", config.host, config.port);
    device->setConnectionParameter(Param::NetworkAddressParameter, QString::fromStdString(config.host));
    device->setConnectionParameter(Param::NetworkPortParameter, config.port);
    setObjectName(QString::fromStdString(fmt::format("Device({}/{})", connectionString, config.name.value)));
}

void radapter::modbus::MasterDevice::Start() {
    if (started) return;
    started = true;
    reconnect->setInterval(int(config.reconnect_timeout_ms));
    reconnect->setSingleShot(true);
    reconnect->callOnTimeout(this, &MasterDevice::doConnect);
    frameGap->setInterval(int(config.frame_gap));
    frameGap->callOnTimeout(this, &MasterDevice::nextReq);
    connect(device, &QModbusClient::stateChanged, this, [this](QModbusClient::State state){
        if (state == QModbusClient::ConnectedState) {
            static_cast<Instance*>(parent())->Info("modbus", "{}: connected", objectName());
            frameGap->start();
            emit ConnectedChanged(true);
        } else if (state == QModbusClient::UnconnectedState) {
            static_cast<Instance*>(parent())->Warn("modbus", "{}: disconnected", objectName());
            frameGap->stop();
            reconnect->start();
            emit ConnectedChanged(false);
        }
    });
    doConnect();
}

void radapter::modbus::MasterDevice::Execute(Op op, Request req) {
    auto& q = op == op_read ? reads : writes;
    auto& max = op == op_read ? config.max_read_queue : config.max_write_queue;
    auto& on_over = op == op_read ? config.on_read_overflow : config.on_write_overflow;
    if (q.size() >= int(max)) {
        static_cast<Instance*>(parent())->Warn("modbus", "{}: {} overflow", objectName(), op_read ? "read" : "write");
        if (on_over == pop_first) {
            q.front().cb({}, std::make_exception_ptr(Err("Removed from queue")));
            q.pop_front();
        } else if (on_over == pop_last) {
            q.back().cb({}, std::make_exception_ptr(Err("Removed from queue")));
            q.pop_back();
        }
    }
    q.push_back(std::move(req));
}

radapter::modbus::MasterDevice::MasterDevice(const Device &conf, QObject *parent) :
    QObject(parent),
    frameGap(new QTimer(this)),
    reconnect(new QTimer(this)),
    config(conf)
{
}

void radapter::modbus::MasterDevice::nextReq() {
    if (busy) {
        return;
    }
    busy = true;
    bool isRead;
    Request req;
    if (writes.size()) {
        req = writes.dequeue();
        isRead = false;
    } else if (reads.size()) {
        req = reads.dequeue();
        isRead = true;
    } else {
        busy = false;
        return;
    }
    auto slave_id = req.slave_id;
    auto ctx = req.ctx;
    auto start = req.unit.startAddress();
    auto len = req.unit.valueCount();
    auto unit = req.unit;
    QModbusReply* reply = isRead
                              ? device->sendReadRequest(unit, req.slave_id)
                              : device->sendWriteRequest(unit, req.slave_id);
    if (!reply) {
        busy = false;
        req.cb({}, std::make_exception_ptr(Err(
                       "{}: error in slave_id({}), registers({}-{}) => Could not send",
                       objectName(), slave_id,
                       start, unsigned(start) + len - 1)));
        return;
    }
    reply->setParent(this);
    connect(reply, &QModbusReply::finished, this, [=, cb = std::move(req.cb)]{
        busy = false;
        if (ctx) {
            if (reply->error()) {
                cb({}, std::make_exception_ptr(Err(
                           "{}: error in slave_id({}), registers({}-{}) => {}",
                           objectName(), slave_id,
                           start, unsigned(start) + len - 1,
                           reply->errorString()))
                   );
            } else {
                auto code = reply->rawResult().functionCode();
                // Qt (5.15) can hand back an empty values list for ReadCoils /
                // ReadDiscreteInputs even on a successful reply, while the raw PDU
                // still carries the packed bits. Rebuild the unit from the raw
                // payload: [byteCount][status bytes...], bits LSB-first from the
                // requested start address.
                if (code == QModbusPdu::ReadCoils || code == QModbusPdu::ReadDiscreteInputs) {
                    auto rdata = reply->rawResult().data();
                    int count = int(unit.valueCount());
                    QModbusDataUnit bits(unit.registerType(), unit.startAddress(), quint16(count));
                    for (int i = 0; i < count; ++i) {
                        int byteIdx = 1 + i / 8; // skip the leading byte-count byte
                        if (byteIdx < rdata.size()) {
                            bits.setValue(i, quint16((quint8(rdata[byteIdx]) >> (i % 8)) & 0x1));
                        }
                    }
                    cb(std::move(bits), {});
                } else {
                    cb(reply->result(), {});
                }
            }
        }
        reply->deleteLater();
    });
}

void radapter::modbus::MasterDevice::doConnect() {
    if (device->state() == QModbusClient::ConnectedState) {
        return;
    }
    static_cast<Instance*>(parent())->Info("modbus", "{}: connecting...", objectName());
    device->connectDevice();
}

radapter::modbus::SlaveDevice::SlaveDevice(RtuDevice config, QObject *parent) :
    SlaveDevice(static_cast<Device&>(config), parent)
{
    using Param = QModbusDevice::ConnectionParameter;
    server = new QModbusRtuSerialServer(this);
    connectionString = config.port;
    server->setConnectionParameter(Param::SerialPortNameParameter, QString::fromStdString(config.port));
    server->setConnectionParameter(Param::SerialBaudRateParameter, config.baud.value);
    server->setConnectionParameter(Param::SerialDataBitsParameter, config.data_bits.value);
    server->setConnectionParameter(Param::SerialStopBitsParameter, config.stop_bits.value);
    server->setConnectionParameter(Param::SerialParityParameter, config.parity.value);
    setObjectName(QString::fromStdString(fmt::format("SlaveDevice({}/{})", connectionString, config.name.value)));
}

radapter::modbus::SlaveDevice::SlaveDevice(TcpDevice config, QObject *parent) :
    SlaveDevice(static_cast<Device&>(config), parent)
{
    using Param = QModbusDevice::ConnectionParameter;
    server = new QModbusTcpServer(this);
    connectionString = fmt::format("{}:{}", config.host, config.port);
    server->setConnectionParameter(Param::NetworkAddressParameter, QString::fromStdString(config.host));
    server->setConnectionParameter(Param::NetworkPortParameter, config.port);
    setObjectName(QString::fromStdString(fmt::format("SlaveDevice({}/{})", connectionString, config.name.value)));
}

radapter::modbus::SlaveDevice::SlaveDevice(const Device &conf, QObject *parent) :
    QObject(parent),
    reconnect(new QTimer(this)),
    config(conf)
{
}

QModbusServer* radapter::modbus::SlaveDevice::Claim(Worker* by) {
    if (claimer) {
        Raise("{}: already claimed by '{}' (created at {})",
              objectName(), claimer->objectName(), claimer->_Origin);
    }
    claimer = by;
    return server;
}

void radapter::modbus::SlaveDevice::Start() {
    if (started) return;
    started = true;
    reconnect->setInterval(int(config.reconnect_timeout_ms));
    reconnect->setSingleShot(true);
    reconnect->callOnTimeout(this, &SlaveDevice::doListen);
    connect(server, &QModbusServer::stateChanged, this, [this](QModbusDevice::State state){
        if (state == QModbusDevice::ConnectedState) {
            static_cast<Instance*>(parent())->Info("modbus", "{}: listening", objectName());
            emit ConnectedChanged(true);
        } else if (state == QModbusDevice::UnconnectedState) {
            static_cast<Instance*>(parent())->Warn("modbus", "{}: stopped", objectName());
            reconnect->start();
            emit ConnectedChanged(false);
        }
    });
    doListen();
}

void radapter::modbus::SlaveDevice::doListen() {
    if (server->state() == QModbusDevice::ConnectedState) {
        return;
    }
    if (!server->connectDevice()) {
        static_cast<Instance*>(parent())->Warn("modbus", "{}: could not start: {}",
                                               objectName(), server->errorString());
        reconnect->start();
    }
}
