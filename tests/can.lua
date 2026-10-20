-- CAN worker self-check on Qt's virtualcan backend: two workers on the same virtual device
-- form a bus, so RX flags, forwarding and the send-side rules are all covered without hardware.

local function new_can()
    return CAN { plugin = "virtualcan", device = "radapter_test_can", bitrate = 500000, can_fd = true, data_bitrate = 2000000 }
end

local can = new_can()
local peer = new_can()

local received = {}
local flushes = 0
pipe(peer, function (msg) received[#received + 1] = msg end)
pipe(can.events, function (msg)
    if msg.flushed then
        flushes = flushes + 1
    end
end)
sleep(300)

local function count_id(id)
    local n = 0
    for _, msg in ipairs(received) do
        if msg.frame_id == id then
            n = n + 1
        end
    end
    return n
end

local function wait_id(id, ms)
    local left = ms
    while left > 0 and count_id(id) == 0 do
        sleep(20)
        left = left - 20
    end
    for _, msg in ipairs(received) do
        if msg.frame_id == id then
            return msg
        end
    end
    return nil
end

-- An empty Lua table is a zero-byte payload, not a map
do
    local before = flushes
    can { frame_id = 0x123, payload = {} }
    local left = 2000
    while left > 0 and flushes == before do
        sleep(20)
        left = left - 20
    end
    assert(flushes > before, "flushed event expected after an empty-payload frame")
end

-- Data frames are the default; remote request frames and FD frames with either BRS are writable
do
    local before = flushes
    for _, frame in ipairs {
        { frame_id = 0x124, payload = "\x01\x02\x03" },
        { frame_id = 0x125, payload = "\x00", frame_type = "request", extended_id = true },
        { frame_id = 0x126, payload = string.rep("\x5a", 64), can_fd = true, bitrate_switch = true },
        { frame_id = 0x127, payload = string.rep("\x5a", 12), can_fd = true, bitrate_switch = false },
    } do
        can(frame)
    end
    sleep(300)
    assert(flushes > before, "a drained batch must emit a flushed event")
end

-- Received frames carry the transport flags and can be forwarded verbatim
do
    can { frame_id = 0x321, payload = string.rep("\x11", 12), can_fd = true, bitrate_switch = true, extended_id = true }
    local frame = wait_id(0x321, 2000)
    assert(frame ~= nil, "the peer worker must receive the frame")
    assert(frame.frame_type == "data" and frame.can_fd == true and frame.bitrate_switch == true
        and frame.extended_id == true and #frame.payload == 12,
        "received frame flags expected, got " .. json_encode(frame))

    local before = count_id(0x321)
    can(frame)
    local left = 2000
    while left > 0 and count_id(0x321) == before do
        sleep(20)
        left = left - 20
    end
    assert(count_id(0x321) > before, "a received frame must be forwardable")
end

-- Non-writable frame types parse (a received frame can be forwarded verbatim) and are rejected
do
    for _, frame_type in ipairs { "error", "unknown", "invalid" } do
        local ok, err = pcall(function () can { frame_id = 0x128, frame_type = frame_type } end)
        assert(not ok and tostring(err):find("only 'data' and 'request'"),
            frame_type .. " frames must be rejected: " .. tostring(err))
    end
end

-- Unknown enum values are rejected with the valid list
do
    local ok, err = pcall(function () can { frame_id = 0x129, frame_type = "bogus" } end)
    assert(not ok and tostring(err):find("Invalid value for enum"), "bad frame_type must be rejected: " .. tostring(err))
end

-- A non-empty map is not a payload, and the message names the keys
do
    local ok, err = pcall(function () can { frame_id = 0x12a, payload = { key = 1 } } end)
    assert(not ok and tostring(err):find("map with keys"), "map payload must be rejected: " .. tostring(err))
end

-- Frame flags that contradict the device configuration are rejected, not silently fixed up
do
    local ok, err = pcall(function () can { frame_id = 0x12b, payload = "\x01", bitrate_switch = true } end)
    assert(not ok and tostring(err):find("requires can_fd"), "BRS without can_fd must be rejected: " .. tostring(err))

    local classic = CAN { plugin = "virtualcan", device = "radapter_test_can", bitrate = 500000 }
    sleep(200)
    ok, err = pcall(function () classic { frame_id = 0x12c, payload = string.rep("\x01", 12), can_fd = true } end)
    assert(not ok and tostring(err):find("opened with can_fd = true"), "FD on a classic device must be rejected: " .. tostring(err))
    classic:shutdown():await()
end

-- os.exit() forwards the code and shuts down gracefully (this also ends the test)
peer:shutdown():await()
can:shutdown():await()
os.exit(0)
