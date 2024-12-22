#pragma once

#include "builtin.hpp"
#include "radapter/config.hpp"

namespace radapter::modbus
{

enum ByteOrder : uint8_t {
    big,
    little,
};
DESCRIBE("modbus::ByteOrder", ByteOrder, void) {
    MEMBER("big", big);
    MEMBER("little", little);
}

enum RegisterMode : uint8_t {
    defaultMode, // initial - if stays this -> not changed by user

    read,
    read_write,
    write,
};
DESCRIBE("modbus::RegisterMode", RegisterMode, void) {
    MEMBER("r", read);
    MEMBER("rw", read_write);
    MEMBER("w", write);
    MEMBER("read", read);
    MEMBER("readwrite", read_write);
    MEMBER("write", write);
}



enum RegisterValueType : uint8_t {
    defaultValueType, // initial - if stays this -> not changed by user

    uint16,
    uint32,
    float32,

    bit,
    Default = uint16,
};
DESCRIBE("modbus::RegisterValueType", RegisterValueType, void) {
    MEMBER("uint16", uint16);
    MEMBER("uint32", uint32);
    MEMBER("float32", float32);
}

struct RegisterPacking {
    WithDefault<ByteOrder> byte = little;
    WithDefault<ByteOrder> word = little;
};
DESCRIBE("modbus::RegisterPacking", RegisterPacking, void) {
    MEMBER("byte", &_::byte);
    MEMBER("word", &_::word);
}

struct Register {
    int index;
    WithDefault<RegisterPacking> packing = {};
    WithDefault<RegisterValueType> type = defaultValueType;
    WithDefault<RegisterMode> mode = defaultMode;
    OptionalPtr<LuaFunction> validator;
};
DESCRIBE("modbus::Register", Register, void) {
    MEMBER("index", &_::index);
    MEMBER("packing", &_::packing);
    MEMBER("type", &_::type);
    MEMBER("mode", &_::mode);
    MEMBER("validator", &_::validator);
}

using SingleTypeMap = map<string, Register>;

struct RegistersMap {
    WithDefault<SingleTypeMap> di;
    WithDefault<SingleTypeMap> holding;
    WithDefault<SingleTypeMap> input;
    WithDefault<SingleTypeMap> coils;
};
DESCRIBE("modbus::RegistersMap", RegistersMap, void) {
    MEMBER("di", &_::di);
    MEMBER("holding", &_::holding);
    MEMBER("input", &_::input);
    MEMBER("coils", &_::coils);
}

enum OverflowBehaviour : uint8_t {
    pop_last,
    pop_first,
};
DESCRIBE("modbus::OverflowBehaviour", OverflowBehaviour, void) {
    MEMBER("pop_last", _::pop_last);
    MEMBER("pop_first", _::pop_first);
}

struct Device {
    WithDefault<string> name = "";
    WithDefault<unsigned> frame_gap = 25u;
    WithDefault<unsigned> max_write_queue = 100u;
    WithDefault<unsigned> max_read_queue = 50u;
    WithDefault<unsigned> reconnect_timeout_ms = 1000u;
    WithDefault<OverflowBehaviour> on_read_overflow = pop_first;
    WithDefault<OverflowBehaviour> on_write_overflow = pop_first;
};
DESCRIBE("modbus::Device", Device, void) {
    MEMBER("name", &_::name);
    MEMBER("frame_gap", &_::frame_gap);
    MEMBER("max_write_queue", &_::max_write_queue);
    MEMBER("max_read_queue", &_::max_read_queue);
    MEMBER("reconnect_timeout_ms", &_::reconnect_timeout_ms);
    MEMBER("on_read_overflow", &_::on_read_overflow);
    MEMBER("on_write_overflow", &_::on_write_overflow);
}

struct RtuDevice : Device {
    string port_name;

    WithDefault<ByteOrder> byte_order = little;
    WithDefault<unsigned> data_bits = 8u;
    WithDefault<unsigned> baud = 115200u;
    WithDefault<unsigned> parity = 0u;
    WithDefault<unsigned> stop_bits = 1u;
};
DESCRIBE("modbus::RtuDevice", RtuDevice, void) {
    PARENT(Device);
    MEMBER("port_name", &_::port_name);
    MEMBER("byte_order", &_::byte_order);
    MEMBER("data_bits", &_::data_bits);
    MEMBER("baud", &_::baud);
    MEMBER("parity", &_::parity);
    MEMBER("stop_bits", &_::stop_bits);
}


struct TcpDevice : Device {
    string host;
    uint16_t port;
};
DESCRIBE("modbus::TcpDevice", TcpDevice, void) {
    PARENT(Device);
    MEMBER("host", &_::host);
    MEMBER("port", &_::port);
}

struct WorkerConfig {
    RegistersMap registers;
    quint16 slave_id;
};
DESCRIBE("modbus::WorkerConfig", WorkerConfig, void) {
    MEMBER("registers", &_::registers);
    MEMBER("slave_id", &_::slave_id);
}

class MasterDevice;

enum RegisterType {
    holding,
    coil,
    di,
    input,
};
DESCRIBE("modbus::RegisterType", RegisterType, void) {
    MEMBER("holding", holding);
    MEMBER("coil", coil);
    MEMBER("di", di);
    MEMBER("discrete_input", di);
    MEMBER("input", input);
}

struct ManualQuery {
    RegisterType type;
    unsigned index;
    unsigned count;
};
DESCRIBE("modbus::ManualQuery", ManualQuery, void) {
    MEMBER("type", &_::type);
    MEMBER("index", &_::index);
    MEMBER("count", &_::count);
}

struct MasterConfig : WorkerConfig {
    MasterDevice* device;
    WithDefault<unsigned> response_time = 150u;
    WithDefault<unsigned> poll_rate = 500u;
    WithDefault<unsigned> write_retries = 3u;
    optional<vector<ManualQuery>> queries = {};
};
DESCRIBE("modbus::MasterConfig", MasterConfig, void) {
    PARENT(WorkerConfig);
    MEMBER("device", &_::device);
    MEMBER("response_time", &_::response_time);
    MEMBER("poll_rate", &_::poll_rate);
    MEMBER("write_retries", &_::write_retries);
    MEMBER("queries", &_::queries);
}

}
