#include "modbus_device.hpp"
#include <QModbusServer>

namespace radapter::modbus
{

struct SlaveConfig : WorkerConfig {
    SlaveDevice* device;
};
DESCRIBE("modbus::SlaveConfig", SlaveConfig, void) {
    PARENT(WorkerConfig);
    MEMBER("device", &_::device);
}

class Slave : public Worker
{
    Q_OBJECT
public:
    SlaveConfig config;
    QModbusServer* server = nullptr;
    PreparedWrites all;
    vector<PreparedRegister> sortedHolding, sortedCoils, sortedDI, sortedInput;
    QMap<string, QVariant> currentState;
    bool applying = false;

    Slave(SlaveConfig conf, Instance* parent) :
        Worker(parent,
               EnsureName(conf, QString("%1/slave:%2")
                                    .arg(conf.device->objectName())
                                    .arg(conf.slave_id)),
               "modbus")
    {
        config = std::move(conf);
        server = config.device->Claim(this);
        validateRegisters(config.registers);
        all = prepareWrites(config.registers, true);
        sortedHolding = prepareReadableSorted(config.registers.holding, true);
        sortedCoils = prepareReadableSorted(config.registers.coils, true);
        sortedDI = prepareReadableSorted(config.registers.di, true);
        sortedInput = prepareReadableSorted(config.registers.input, true);

        server->setServerAddress(config.slave_id);
        QModbusDataUnitMap map;
        auto addRange = [&](vector<PreparedRegister> const& regs, QModbusDataUnit::RegisterType t) {
            if (regs.empty()) return;
            auto lo = regs.front().index;
            auto hi = regs.back().index + regs.back().sizeOf / 2;
            map.insert(t, QModbusDataUnit{t, lo, uint16_t(hi - lo)});
        };
        addRange(sortedHolding, QModbusDataUnit::HoldingRegisters);
        addRange(sortedCoils, QModbusDataUnit::Coils);
        addRange(sortedDI, QModbusDataUnit::DiscreteInputs);
        addRange(sortedInput, QModbusDataUnit::InputRegisters);
        server->setMap(map);

        connect(server, &QModbusServer::dataWritten, this, &Slave::onDataWritten);
        config.device->Start();
    }

    void onDataWritten(QModbusDataUnit::RegisterType type, int address, int size) {
        if (applying) return;
        auto* sorted = [&]() -> vector<PreparedRegister>* {
            switch (type) {
            case QModbusDataUnit::HoldingRegisters: return &sortedHolding;
            case QModbusDataUnit::Coils: return &sortedCoils;
            case QModbusDataUnit::DiscreteInputs: return &sortedDI;
            case QModbusDataUnit::InputRegisters: return &sortedInput;
            default: return nullptr;
            }
        }();
        if (!sorted) return;
        FlatMap diff;
        uint16_t data[2];
        for (auto& reg: *sorted) {
            if (reg.index + reg.sizeOf / 2 <= address) continue;
            if (reg.index >= address + size) break;
            data[0] = data[1] = 0;
            server->data(type, uint16_t(reg.index), &data[0]);
            if (reg.sizeOf == 4) {
                server->data(type, uint16_t(reg.index + 1), &data[1]);
            }
            if (reg.type == bit && data[0] > 1) {
                // FC05 coil-on arrives as the raw 0xFF00 payload: normalize
                // the storage too, so master reads see a proper bit
                data[0] = 1;
                applying = true;
                server->setData(type, uint16_t(reg.index), 1);
                applying = false;
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
            auto it = all.find(k);
            if (it == all.end()) continue;
            auto& reg = it->second;
            QVector<uint16_t> words;
            if (!encodeRegister(reg, v, words)) {
                Warn("could not encode '{}' <= {}", k, v.toString());
                continue;
            }
            QModbusDataUnit unit;
            unit.setRegisterType(reg.mbType);
            unit.setStartAddress(reg.index);
            unit.setValues(words);
            applying = true;
            auto ok = server->setData(unit);
            applying = false;
            if (!ok) {
                Warn("could not set '{}': {}", k, server->errorString());
            } else {
                currentState[k] = v;
            }
        }
    }
};

void RegisterSlave(Instance* inst) {
    inst->RegisterWorker<Slave>("ModbusSlave");
    inst->RegisterSchema("ModbusSlave", SchemaFor<SlaveConfig>);
}

}

#include "modbus_slave.moc"
