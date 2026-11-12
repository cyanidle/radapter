-- Cyphal worker self-check on Qt's virtualcan backend: two nodes on one virtual
-- bus exchange a message each way, and a GetInfo service round-trip has to carry
-- the server's node info back. The second half is the regression guard: the
-- GetInfo *request* has a zero-byte extent, so a response subscription sized
-- from it truncates the answer to all-default fields (and a port-only dispatch
-- used to crash the requester instead of answering at all).

local DEVICE = "radapter_test_cyphal"

local function new_can()
    return CAN {
        plugin = "virtualcan", device = DEVICE,
        bitrate = 1000000, data_bitrate = 5000000, can_fd = true,
    }
end

local server = Cyphal {
    can = new_can(),
    node_id = 60,
    node_info = { name = "cyphal-test-server", software_version = { major = 7, minor = 3 } },
    publish = { out = { type = "uavcan.primitive.scalar.Real32.1.0", port = 2001 } },
    subscribe = { inbound = { type = "uavcan.primitive.scalar.Real32.1.0", port = 2002 } },
}
local client = Cyphal {
    can = new_can(),
    node_id = 61,
    node_info = { name = "cyphal-test-client" },
    subscribe = { out = { type = "uavcan.primitive.scalar.Real32.1.0", port = 2001 } },
    publish = { inbound = { type = "uavcan.primitive.scalar.Real32.1.0", port = 2002 } },
}

local got, back = {}, {}
on(client, "out", function(msg) got[#got + 1] = msg.value end)
on(server, "inbound", function(msg) back[#back + 1] = msg.value end)
sleep(500)

server { out = { value = 1.25 } }
sleep(300)
assert(#got > 0 and math.abs(got[#got] - 1.25) < 1e-6, "no message reached the client")

client { inbound = { value = -0.5 } }
sleep(300)
assert(#back > 0 and math.abs(back[#back] + 0.5) < 1e-6, "no message reached the server")

local res, err = client:Request({ type = "uavcan.node.GetInfo.1.0", server = 60, port = 430 }, {}):await()
assert(res ~= nil and err == nil, "GetInfo was not answered: " .. tostring(err))
assert(res.name == "cyphal-test-server", "GetInfo lost the node name: " .. tostring(res.name))
assert(res.software_version and res.software_version.major == 7 and res.software_version.minor == 3,
    "GetInfo lost the software version")
assert(res.protocol_version and res.protocol_version.major == 1,
    "GetInfo lost the protocol version")
assert(res.unique_id ~= nil and #tostring(res.unique_id) > 0, "GetInfo lost the unique id")

local res2, err2 = server:Request({ type = "uavcan.node.GetInfo.1.0", server = 61, port = 430 }, {}):await()
assert(res2 ~= nil and err2 == nil, "GetInfo the other way was not answered: " .. tostring(err2))
assert(res2.name == "cyphal-test-client", "GetInfo lost the client's name: " .. tostring(res2.name))

-- A request sent after the port has gone idle must not reuse a transfer id the
-- server still remembers: it would treat the request as a duplicate and stay
-- silent, so this answers only while the port keeps its transfer-id counter.
for _, gap in ipairs({ 1200, 1200, 700 }) do
    sleep(gap)
    local res3, err3 = client:Request({ type = "uavcan.node.GetInfo.1.0", server = 60, port = 430 },
        { timeout = 2000 }):await()
    assert(res3 ~= nil and err3 == nil,
        ("a GetInfo after a %d ms idle gap timed out: %s"):format(gap, tostring(err3)))
    assert(res3.name == "cyphal-test-server", "the spaced GetInfo lost the node name")
end

print("can_cyphal: ok")
os.exit(0)
