#include "modbus_device.hpp"

radapter::modbus::MasterDevice::MasterDevice(RtuDevice config, QObject *parent) :
    MasterDevice(static_cast<Device&>(config), parent)
{
    using Param = QModbusRtuSerialMaster::ConnectionParameter;
    auto dev = new QModbusRtuSerialMaster(this);
    dev->setInterFrameDelay(int(config.frame_gap.value));
    device = dev;
    connectionString = config.port_name;
    device->setConnectionParameter(Param::SerialBaudRateParameter, config.baud.value);
    device->setConnectionParameter(Param::SerialDataBitsParameter, config.data_bits.value);
    device->setConnectionParameter(Param::SerialStopBitsParameter, config.stop_bits.value);
    device->setConnectionParameter(Param::SerialParityParameter, config.parity.value);
    device->setConnectionParameter(Param::SerialPortNameParameter, QString::fromStdString(config.port_name));
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
}

void radapter::modbus::MasterDevice::Start() {
    if (started) return;
    started = true;
    setObjectName(QString::fromStdString(fmt::format("Device({}/{})", connectionString, config.name.value)));
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
    QModbusReply* reply = isRead
                              ? device->sendReadRequest(req.unit, req.slave_id)
                              : device->sendWriteRequest(req.unit, req.slave_id);
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
                cb(reply->result(), {});
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
