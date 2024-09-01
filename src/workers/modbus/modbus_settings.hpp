#pragma once

#include "builtin.hpp"
#include "config.hpp"

namespace radapter::modbus
{

enum ByteOrder : uint8_t {
    big,
    little,
};
DESCRIBE(modbus::ByteOrder, big, little)

enum RegisterMode : uint8_t {
    defaultMode, // initial - if stays this -> not changed by user

    r,
    rw,
    w,
};
DESCRIBE(modbus::RegisterMode, r, rw, w)

enum RegisterValueType : uint8_t {
    defaultValueType, // initial - if stays this -> not changed by user

    uint16,
    uint32,
    float32,

    bit,
    Default = uint16,
};
DESCRIBE(modbus::RegisterValueType, uint16, uint32, float32)

struct RegisterPacking {
    WithDefault<ByteOrder> byte = little;
    WithDefault<ByteOrder> word = little;
};
DESCRIBE(modbus::RegisterPacking, &_::byte, &_::word)

struct Register {
    int index;
    WithDefault<RegisterPacking> packing = {};
    WithDefault<RegisterValueType> type = defaultValueType;
    WithDefault<RegisterMode> mode = defaultMode;
    OptionalPtr<LuaFunction> validator;
};

DESCRIBE(modbus::Register, &_::packing, &_::type, &_::index,
         &_::mode, &_::validator)

using SingleTypeMap = map<string, Register>;

struct RegistersMap {
    WithDefault<SingleTypeMap> di;
    WithDefault<SingleTypeMap> holding;
    WithDefault<SingleTypeMap> input;
    WithDefault<SingleTypeMap> coils;
};
DESCRIBE(modbus::RegistersMap, &_::di, &_::holding, &_::input, &_::coils)

enum OverflowBehaviour : uint8_t {
    pop_last,
    pop_first,
};
DESCRIBE(modbus::OverflowBehaviour, pop_last, pop_first)

struct Device {
    WithDefault<string> name = "";
    WithDefault<unsigned> frame_gap = 25u;
    WithDefault<unsigned> max_write_queue = 100u;
    WithDefault<unsigned> max_read_queue = 50u;
    WithDefault<unsigned> reconnect_timeout_ms = 1000u;
    WithDefault<OverflowBehaviour> on_read_overflow = pop_first;
    WithDefault<OverflowBehaviour> on_write_overflow = pop_first;
};
DESCRIBE(modbus::Device, &_::name,
         &_::frame_gap, &_::max_write_queue,
         &_::max_read_queue, &_::reconnect_timeout_ms,
         &_::on_read_overflow, &_::on_write_overflow)

struct RtuDevice : Device {
    string port_name;

    WithDefault<ByteOrder> byte_order = little;
    WithDefault<unsigned> data_bits = 8u;
    WithDefault<unsigned> baud = 115200u;
    WithDefault<unsigned> parity = 0u;
    WithDefault<unsigned> stop_bits = 1u;
};
DESCRIBE_INHERIT(modbus::RtuDevice, Device,
         &_::port_name, &_::byte_order,
         &_::data_bits, &_::baud,
         &_::parity, &_::stop_bits)


struct TcpDevice : Device {
    string host;
    uint16_t port;
};
DESCRIBE_INHERIT(modbus::TcpDevice, Device, &_::host, &_::port)

struct WorkerConfig {
    RegistersMap registers;
    quint16 slave_id;
};
DESCRIBE(modbus::WorkerConfig, &_::registers, &_::slave_id)

class MasterDevice;

enum RegisterType {
    holding,
    coil,
    di,
    input,
};
DESCRIBE(modbus::RegisterType, holding, coil, di, input)

struct ManualQuery {
    RegisterType type;
    unsigned index;
    unsigned count;
};
DESCRIBE(modbus::ManualQuery, &_::type, &_::index, &_::count)

struct MasterConfig : WorkerConfig {
    MasterDevice* device;
    WithDefault<unsigned> response_time = 150u;
    WithDefault<unsigned> poll_rate = 500u;
    WithDefault<unsigned> write_retries = 3u;
    optional<vector<ManualQuery>> queries = {};
};
DESCRIBE_INHERIT(modbus::MasterConfig, WorkerConfig,
                 &_::device, &_::response_time,
                 &_::poll_rate, &_::write_retries, &_::queries)

}
