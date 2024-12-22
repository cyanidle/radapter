#pragma once
#include "modbus_settings.hpp"
#include "qmodbusdataunit.h"

namespace radapter::modbus
{

struct PreparedRegister {
    string key;
    RegisterValueType type{};
    RegisterPacking packing{};
    optional<LuaFunction> validator;
    int index{};
    int sizeOf{};
};

struct PreparedWriteRegister : PreparedRegister {
    bool writeOnly{};
    QModbusDataUnit::RegisterType mbType{};
};

struct MergedRead {
    vector<PreparedRegister> regs;
    QModbusDataUnit unit;
};

using PreparedReads = vector<MergedRead>;

using RegPair = std::pair<string, Register>;

static int getSizeOf(RegisterValueType type) {
    switch (type) {
    case defaultValueType:
    case bit: return 2; //still a 'word'
    case uint16: return 2;
    case float32: return 4;
    case uint32: return 4;
    }
    Q_UNREACHABLE();
}

template<typename T>
static bool compareByIndex(T const& lhs, T const& rhs) noexcept {
    return lhs.index < rhs.index;
}

static void validateReadOnly(string_view key, Register& reg, string_view type) {
    if (reg.mode == write || reg.mode == read_write) {
        throw Err("Register {}: {} registers cannot be writable", key, type);
    }
}

static void validateSingleBit(string_view key, Register& reg, string_view type) {
    if (reg.mode == write || reg.mode == read_write) {
        throw Err("Register {}: {} registers cannot have a type (single bit only)", key, type);
    }
}

[[maybe_unused]]
static void validateRegisters(RegistersMap& regs) {
    for (auto& [k, v]: regs.di.value) {
        validateReadOnly(k, v, "Discrete Input");
        validateSingleBit(k, v, "Discrete Input");
        v.mode = read;
        v.type = bit;
    }
    for (auto& [k, v]: regs.coils.value) {
        validateSingleBit(k, v, "Coils");
        v.type = bit;
    }
    for (auto& [k, v]: regs.input.value) {
        validateReadOnly(k, v, "Input");
        if (v.mode == defaultMode) {
            v.mode = read_write;
        }
        if (v.type == defaultValueType) {
            v.type = uint16;
        }
    }
    for (auto& [k, v]: regs.holding.value) {
        if (v.mode == defaultMode) {
            v.mode = read_write;
        }
        if (v.type == defaultValueType) {
            v.type = uint16;
        }
    }
}

static vector<PreparedRegister> prepareReadableSorted(SingleTypeMap const& map) {
    vector<PreparedRegister> sorted;
    for (auto& [k, reg]: map) {
        if (reg.mode == write) {
            continue; //write-only register
        }
        PreparedRegister meta;
        meta.index = reg.index;
        meta.key = k;
        meta.type = reg.type;
        if (reg.validator) {
            meta.validator = *reg.validator.value;
        }
        meta.sizeOf = getSizeOf(reg.type);
        meta.packing = reg.packing;
        auto it = std::lower_bound(sorted.begin(), sorted.end(), meta, compareByIndex<PreparedRegister>);
        if (it == sorted.end()) {
            sorted.push_back(std::move(meta));
        } else if (it->index == meta.index) {
            throw Err("Index collision for registers '{}' and '{}' @ index {}",
                      it->key, k, it->index);
        } else if (meta.index + meta.sizeOf/2 > it->index) {
            throw Err("Register overlap of '{}' and '{}' @ index {} (+ {} > {})",
                      k, it->key, meta.index, meta.sizeOf / 2, it->index);
        } else {
            sorted.insert(it, std::move(meta));
        }
    }
    return sorted;
}

static void mergeSingle(
    vector<MergedRead> & out,
    SingleTypeMap const& map,
    QModbusDataUnit::RegisterType type)
{
    auto sorted = prepareReadableSorted(map);
    MergedRead merge;
    for (auto& reg: sorted) {
        auto setAsFirst = [&](auto& reg){
            merge.regs.push_back(std::move(reg));
            merge.unit.setRegisterType(type);
            merge.unit.setStartAddress(reg.index);
            merge.unit.setValueCount(uint16_t(reg.sizeOf/2));
        };
        if (merge.regs.empty()) {
            setAsFirst(reg);
            continue;
        }
        auto lastIdx = merge.regs.back().index;
        auto lastCount = merge.regs.back().sizeOf/2;
        if (reg.index == lastIdx + lastCount) {
            merge.regs.push_back(std::move(reg));
            merge.unit.setValueCount(merge.unit.valueCount() + uint16_t(reg.sizeOf/2));
        } else {
            out.push_back(std::move(merge));
            setAsFirst(reg);
        }
    }
    if (!merge.regs.empty()) {
        out.push_back(std::move(merge));
    }
}

[[maybe_unused]]
static PreparedReads prepareReads(RegistersMap const& map) {
    PreparedReads result;
    mergeSingle(result, map.holding.value, QModbusDataUnit::HoldingRegisters);
    mergeSingle(result, map.di.value, QModbusDataUnit::DiscreteInputs);
    mergeSingle(result, map.input.value, QModbusDataUnit::InputRegisters);
    mergeSingle(result, map.coils.value, QModbusDataUnit::Coils);
    return result;
}

static MergedRead prepareSingleManual(
    ManualQuery const& q,
    vector<PreparedRegister> const& regs,
    QModbusDataUnit::RegisterType type)
{
    MergedRead read;
    read.unit.setRegisterType(type);
    read.unit.setStartAddress(int(q.index));
    read.unit.setValueCount(q.count);
    auto end = q.index + q.count;
    for (auto& r: regs) {
        auto regEnd = r.index + r.sizeOf;
        if (r.index >= int(q.index) && r.index < int(end)) {
            if (regEnd > int(end)) {
                string_view name;
                (void)describe::enum_to_name(q.type, name);
                throw Err("Register {}: Manual query ({} {}-{}) would split register (it is in range of {}-{})",
                          r.key, name, q.index, end, r.index, regEnd);
            }
            read.regs.push_back(r);
        } else if (r.index >= int(end)) {
            break; // regs is sorted by index, we can skip the rest
        }
    }
    return read;
}

[[maybe_unused]]
static PreparedReads prepareManualReads(RegistersMap const& map, vector<ManualQuery> const& qs) {
    PreparedReads result;
    auto sortedHoldings = prepareReadableSorted(map.holding);
    auto sortedCoils = prepareReadableSorted(map.coils);
    auto sortedDI = prepareReadableSorted(map.di);
    auto sortedInputs = prepareReadableSorted(map.input);
    for (auto& q: qs) {
        result.push_back([&]{
            switch (q.type) {
            case holding: return prepareSingleManual(q, sortedHoldings, QModbusDataUnit::HoldingRegisters);
            case coil: return prepareSingleManual(q, sortedCoils, QModbusDataUnit::Coils);
            case di: return prepareSingleManual(q, sortedDI, QModbusDataUnit::DiscreteInputs);
            case input: return prepareSingleManual(q, sortedInputs, QModbusDataUnit::InputRegisters);
            }
            Q_UNREACHABLE();
        }());
    }
    return result;
}

using PreparedWrites = map<string, PreparedWriteRegister>;

[[maybe_unused]]
static PreparedWrites prepareWrites(RegistersMap const& map) {
    PreparedWrites result;
    vector<PreparedWriteRegister> regs;
    auto add = [&](SingleTypeMap const& src, QModbusDataUnit::RegisterType t) {
        regs.clear();
        for (auto& [k, r]: src) {
            if (r.mode == RegisterMode::read) {
                continue;
            }
            PreparedWriteRegister reg;
            reg.index = r.index;
            reg.writeOnly = r.mode == RegisterMode::write;
            reg.mbType = t;
            reg.key = k;
            if (r.validator) {
                reg.validator = *r.validator.value;
            }
            reg.packing = r.packing;
            reg.type = r.type;
            reg.sizeOf = getSizeOf(reg.type);
            auto it = std::lower_bound(regs.begin(), regs.end(),
                                       reg, compareByIndex<PreparedWriteRegister>);
            if (it == regs.end()) {
                regs.push_back(std::move(reg));
            } else if (reg.index + reg.sizeOf/2 > it->index) {
                throw Err("Register overlap of '{}' and '{}' @ index {} (+ {} > {})",
                          reg.key, it->key, reg.index,
                          reg.sizeOf/2, it->index);
            } else {
                regs.insert(it, std::move(reg));
            }
        }
        for (auto& r: regs) {
            result[r.key] = std::move(r);
        }
    };
    add(map.coils.value, QModbusDataUnit::Coils);
    add(map.di.value, QModbusDataUnit::DiscreteInputs);
    add(map.holding.value, QModbusDataUnit::HoldingRegisters);
    add(map.input.value, QModbusDataUnit::InputRegisters);
    return result;
}

}
