#include "modbus_device.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

namespace radapter::modbus
{

class Master : public Worker
{
    Q_OBJECT
public:
    MasterConfig config;
    PreparedReads reads;
    PreparedWrites writable;
    struct InFlight {
        QModbusDataUnit unit;
        unsigned retriesLeft{};
    };
    std::unordered_map<string, InFlight> inFlight;
    QMap<string, QVariant> currentState;
    Master(MasterConfig conf, Instance* parent) :
        Worker(parent,
               EnsureName(conf, QString("%1/slave:%2")
                                    .arg(conf.device->objectName())
                                    .arg(conf.slave_id)),
               "modbus")
    {
        config = std::move(conf);
        validateRegisters(config.registers);
        if (config.queries) {
            reads = prepareManualReads(config.registers, *config.queries);
        } else {
            reads = prepareReads(config.registers);
        }
        writable = prepareWrites(config.registers);
        if (TagsEnabled()) {
            QStringList fields;
            for (auto& merged : reads)
                for (auto& reg : merged.regs)
                    fields << QString::fromStdString(reg.key);
            AdvertiseFields(fields);
        }
        QTimer* poller = new QTimer(this);
        poller->setInterval(int(config.poll_rate));
        poller->callOnTimeout(this, &Master::poll);
        connect(config.device, &MasterDevice::ConnectedChanged, this, [=](bool state){
            if (state) {
                poller->start();
                emit SendEvent(QVariantMap{{"state", "ConnectedState"}});
            } else {
                poller->stop();
                emit SendEvent(QVariantMap{{"state", "UnconnectedState"}});
            }
        });
        config.device->Start();
    }
    void poll() {
        for (auto& merged: reads) {
            Request req;
            req.slave_id = config.slave_id;
            req.unit = merged.unit;
            req.ctx = this;
            req.cb = [this, src = &merged](QModbusDataUnit result, std::exception_ptr except){
                if (except) {
                    try {
                        std::rethrow_exception(except);
                    } catch (std::exception& e) {
                        Error("Error reading: {}", e.what());
                        return;
                    }
                }
                parsePoll(*src, result);
            };
            config.device->Execute(MasterDevice::op_read, std::move(req));
        }
    }
    void parsePoll(MergedRead const& read, QModbusDataUnit const& resp) {
        uint16_t data[2];
        FlatMap diff;
        for (auto& reg: read.regs) {
            data[0] = resp.value(reg.index - resp.startAddress());
            if (reg.sizeOf == 4) {
                data[1] = resp.value(reg.index + 1 - resp.startAddress());
            }
            auto asVariant = decodeRegister(reg, data);
            auto& current = currentState[reg.key];
            if (current != asVariant) {
                current = std::move(asVariant);
                diff.push_back({reg.key, current});
            }
        }
        if (!diff.empty()) {
            QVariant unflat;
            Unflatten(unflat, diff);
            emit SendMsg(unflat);
        }
    }
    void OnMsg(QVariant const& msg) override {
        FlatMap flat;
        Flatten(flat, msg);
        for (auto& [k, v]: flat) {
            if (!v.isValid()) continue;
            auto it = writable.find(k);
            if (it == writable.end()) continue; //warn?
            auto& reg = it->second;
            if (reg.validator) {
                try {
                    auto res = reg.validator->Call({});
                    if (!res.value<bool>()) {
                        Debug("Writing {} with => {} failed validation", k, v.toString());
                        continue;
                    }
                } catch (std::exception& e) {
                    Warn("Error calling validator for {}: {}", k, e.what());
                    continue;
                }
            }
            QVector<uint16_t> words;
            if (!encodeRegister(reg, v, words)) {
                Warn("could not encode '{}' <= {}", k, v.toString());
                continue;
            }
            QModbusDataUnit unit;
            unit.setRegisterType(reg.mbType);
            unit.setStartAddress(reg.index);
            unit.setValues(words);
            inFlight[k] = {unit, config.write_retries};
            write(std::move(unit), &reg, std::move(v));
        }
    }
    void write(QModbusDataUnit const& unit, const PreparedWriteRegister* reg, QVariant v) {
        Request req;
        req.unit = unit;
        req.ctx = this;
        req.slave_id = config.slave_id;
        req.cb = [this, reg, v = reg->writeOnly ? std::move(v) : QVariant{}](QModbusDataUnit, std::exception_ptr except) mutable {
            if (except) {
                try {
                    std::rethrow_exception(except);
                } catch (std::exception& e) {
                    Warn("error writing '{}': {}", reg->key, e.what());
                    retry(reg, std::move(v));
                }
            } else {
                ok(reg, std::move(v));
            }
        };
        config.device->Execute(MasterDevice::op_write, std::move(req));
    }
    void retry(const PreparedWriteRegister* reg, QVariant v) {
        Info("retrying '{}'", reg->key);
        auto it = inFlight.find(reg->key);
        if (it == inFlight.end()) {
            Error("inflight record for '{}' lost", reg->key);
            return;
        }
        if (it->second.retriesLeft == 0) {
            Error("could not write '{}' for {} times",
                    reg->key, config.write_retries.value);
            return;
        }
        it->second.retriesLeft--;
        write(it->second.unit, reg, std::move(v));
    }
    void ok(const PreparedWriteRegister* reg, QVariant v) {
        if (reg->writeOnly) {
            QVariant diff;
            Unflatten(diff, {{reg->key, std::move(v)}});
            emit SendMsg(diff);
        }
        inFlight.erase(reg->key);
    }
};

}

template<typename Device, typename T>
static QVariant makeDevice(radapter::Instance* inst, QVariantList args) {
    using namespace radapter;
    T dev;
    Parse(dev, args.value(0));
    QObject* device = new Device(std::move(dev), inst);
    return QVariant::fromValue(device);
}

void radapter::builtin::workers::modbus(Instance* inst) {
    inst->RegisterFunc("TcpModbusDevice", makeDevice<modbus::MasterDevice, modbus::TcpDevice>);
    inst->RegisterSchema("TcpModbusDevice", SchemaFor<modbus::TcpDevice>);

    inst->RegisterFunc("RtuModbusDevice", makeDevice<modbus::MasterDevice, modbus::RtuDevice>);
    inst->RegisterSchema("RtuModbusDevice", SchemaFor<modbus::RtuDevice>);

    inst->RegisterFunc("TcpModbusServer", makeDevice<modbus::SlaveDevice, modbus::TcpDevice>);
    inst->RegisterSchema("TcpModbusServer", SchemaFor<modbus::TcpDevice>);

    inst->RegisterFunc("RtuModbusServer", makeDevice<modbus::SlaveDevice, modbus::RtuDevice>);
    inst->RegisterSchema("RtuModbusServer", SchemaFor<modbus::RtuDevice>);

    inst->RegisterWorker<modbus::Master>("ModbusMaster");
    inst->RegisterSchema("ModbusMaster", SchemaFor<modbus::MasterConfig>);

    modbus::RegisterSlave(inst);
}

#include "modbus.moc"
