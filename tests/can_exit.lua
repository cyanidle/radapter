-- os.exit() from a CAN listener must tear the workers down without crashing and exit
-- cleanly (forwarding of non-zero codes is covered by tests/exit_code.cmake). The chunk
-- returns immediately; the process is expected to end from the listener, so a listener
-- that never runs is caught by the ctest timeout.

local a = CAN { plugin = "virtualcan", device = "radapter_test_can", bitrate = 500000 }
local b = CAN { plugin = "virtualcan", device = "radapter_test_can", bitrate = 500000 }

pipe(b, function (msg)
    log("exiting from a listener on frame {}", msg.frame_id)
    os.exit(0)
end)
sleep(300)

a { frame_id = 0x10, payload = "\x01" }
