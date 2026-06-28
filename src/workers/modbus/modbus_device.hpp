#pragma once
#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include "modbus_settings.hpp"
#include <QModbusDevice>
#include <QModbusTcpClient>
#include <QTimer>
#include <QQueue>
#include <QPointer>
#include <QtEndian>
#include <QModbusRtuSerialClient>
#include "modbus_units.hpp"

class QModbusServer;

namespace radapter::modbus
{

using Callback = std::function<void(QModbusDataUnit ok, std::exception_ptr except)>;

struct Request {
    QPointer<QObject> ctx;
    int slave_id = 0;
    QModbusDataUnit unit;
    Callback cb;
};

class MasterDevice : public QObject {
    Q_OBJECT

    bool started = false;
    bool busy = false;
    QTimer* frameGap = nullptr;
    QTimer* reconnect = nullptr;
    QModbusClient* device = nullptr;
    QQueue<Request> reads;
    QQueue<Request> writes;
    Device config;
    string connectionString;
public:
    MasterDevice(RtuDevice config, QObject* parent);
    MasterDevice(TcpDevice config, QObject* parent);
    void Start();

    enum Op {
        op_read,
        op_write,
    };

    void Execute(Op op, Request req);
signals:
    void ConnectedChanged(bool state);
private:
    MasterDevice(Device const& conf, QObject* parent);
    void nextReq();
    void doConnect();
};

class SlaveDevice : public QObject {
    Q_OBJECT

    bool started = false;
    QPointer<Worker> claimer;
    QTimer* reconnect = nullptr;
    ::QModbusServer* server = nullptr;
    Device config;
    string connectionString;
public:
    SlaveDevice(RtuDevice config, QObject* parent);
    SlaveDevice(TcpDevice config, QObject* parent);

    //! a server can host exactly one ModbusSlave (single server address + map)
    ::QModbusServer* Claim(Worker* by);
    void Start();
signals:
    void ConnectedChanged(bool state);
private:
    SlaveDevice(Device const& conf, QObject* parent);
    void doListen();
};

}
