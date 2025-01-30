#include "modbus_device.hpp"

namespace radapter::modbus
{

static void applyPacking(QVector<uint16_t>& words, RegisterPacking pack) {
    auto self = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? little : big;
    if (words.size() == 2 && self != pack.word) {
        std::swap(words[0], words[1]);
    }
    for (auto& word: words) {
        if (pack.byte == little) {
            word = qToLittleEndian(word);
        } else {
            word = qToBigEndian(word);
        }
    }
}

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
    Master(QVariantList const& conf, Instance* parent) : Worker(parent, "modbus") {
        Parse(config, conf.value(0));
        validateRegisters(config.registers);
        if (config.queries) {
            reads = prepareManualReads(config.registers, *config.queries);
        } else {
            reads = prepareReads(config.registers);
        }
        writable = prepareWrites(config.registers);
        QTimer* poller = new QTimer(this);
        poller->setInterval(int(config.poll_rate));
        poller->callOnTimeout(this, &Master::poll);
        connect(config.device, &MasterDevice::ConnectedChanged, this, [=](bool state){
            if (state) {
                poller->start();
            } else {
                poller->stop();
            }
        });
        config.device->Start();
        setObjectName(QString("%2_Master(%1)")
                          .arg(config.slave_id)
                          .arg(config.device->objectName()));
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
    template<typename T>
    static T parseType(RegisterPacking packing, const void* val) {
        if (packing.byte == big) {
            return qFromBigEndian<T>(val);
        } else {
            return qFromLittleEndian<T>(val);
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
            constexpr auto self = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? little : big;
            if (reg.packing.word != self) {
                std::swap(data[0], data[1]);
            }
            auto asVariant = [&]() -> QVariant {
                switch (reg.type) {
                case defaultValueType: assert(false && "lib error"); std::abort();
                case bit:
                case uint16: return parseType<uint16_t>(reg.packing, data);
                case uint32: return parseType<uint32_t>(reg.packing, data);
                case float32: return parseType<float>(reg.packing, data);
                }
                return {};
            }();
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
                    auto res = (*reg.validator)({});
                    if (!res.value<bool>()) {
                        Debug("Writing {} with => {} failed validation", k, v.toString());
                        continue;
                    }
                } catch (std::exception& e) {
                    Warn("Error calling validator for {}: {}", k, e.what());
                    continue;
                }
            }
            QModbusDataUnit unit;
            unit.setRegisterType(reg.mbType);
            unit.setStartAddress(reg.index);
            unit.setValueCount(uint32_t(reg.sizeOf/2));
            QVector<uint16_t> words;
            switch (reg.type) {
            case defaultValueType: 
                assert(false && "lib error"); 
                std::abort();
            case bit: {
                words.push_back(uint16_t(v.toBool()));
                break;
            }
            case uint16: {
                bool ok;
                auto ui = v.toUInt(&ok);
                if (!ok || ui > (std::numeric_limits<uint16_t>::max)()) {
                    continue; //warn?
                }
                words.push_back(uint16_t(ui));
                applyPacking(words, reg.packing);
                break;
            }
            case uint32:  {
                bool ok;
                uint32_t ui = v.toUInt(&ok);
                if (!ok || ui > (std::numeric_limits<uint16_t>::max)()) {
                    continue; //warn?
                }
                words.resize(2);
                memcpy(words.data(), &ui, sizeof(ui));
                applyPacking(words, reg.packing);
                break;
            }
            case float32:  {
                bool ok;
                float f = v.toFloat(&ok);
                if (!ok) {
                    continue; //warn?
                }
                words.resize(2);
                memcpy(words.data(), &f, sizeof(f));
                applyPacking(words, reg.packing);
                break;
            }
            }
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
                    Warn("{}: error writing '{}'", objectName(), reg->key);
                    retry(reg, std::move(v));
                }
            } else {
                ok(reg, std::move(v));
            }
        };
        config.device->Execute(MasterDevice::op_write, std::move(req));
    }
    void retry(const PreparedWriteRegister* reg, QVariant v) {
        Info("{}: retrying '{}'", objectName(), reg->key);
        auto it = inFlight.find(reg->key);
        if (it == inFlight.end()) {
            Error("{}: inflight record for '{}' lost", objectName(), reg->key);
            return;
        }
        if (it->second.retriesLeft == 0) {
            Error("{}: could not write '{}' for {} times",
                    objectName(), reg->key, config.write_retries.value);
            return;
        }
        it->second.retriesLeft--;
        write(it->second.unit, reg, std::move(v));
    }
    void ok(const PreparedWriteRegister* reg, QVariant v) {
        Debug("{}: ok '{}' => {}", objectName(), reg->key, v.toString());
        if (reg->writeOnly) {
            QVariant diff;
            Unflatten(diff, {{reg->key, std::move(v)}});
            emit SendMsg(diff);
        }
        inFlight.erase(reg->key);
    }
};

}

template<typename T>
static QVariant makeDevice(radapter::Instance* inst, QVariantList args) {
    using namespace radapter;
    T dev;
    Parse(dev, args.value(0));
    QObject* device = new modbus::MasterDevice(std::move(dev), inst);
    return QVariant::fromValue(device);
}

void radapter::builtin::workers::modbus(Instance* inst) {
    inst->RegisterFunc("TcpModbusDevice", makeDevice<modbus::TcpDevice>);
    inst->RegisterSchema("TcpModbusDevice", SchemaFor<modbus::TcpDevice>);

    inst->RegisterFunc("RtuModbusDevice", makeDevice<modbus::RtuDevice>);
    inst->RegisterSchema("RtuModbusDevice", SchemaFor<modbus::RtuDevice>);

    inst->RegisterWorker<modbus::Master>("ModbusMaster");
    inst->RegisterSchema("ModbusMaster", SchemaFor<modbus::MasterConfig>);
}

#include "modbus.moc"
