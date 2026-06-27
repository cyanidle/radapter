# radapter

**NodeJS-style plumbing for industrial/embedded integration.**  
Wire together Modbus, WebSocket, Redis, SQL, Serial, CAN, and QML GUI using short Lua scripts — with schema validation, async/await, and hot-reload.

## Why

Integrating industrial devices usually means writing a full application: connect the device, parse the protocol, convert data, push it somewhere, repeat. radapter turns that into a pipeline:

```lua
local dev = TcpModbusDevice { host = "192.168.1.10", port = 502 }

local plc = ModbusMaster {
    device = dev, slave_id = 1,
    registers = {
        holding = {
            ["pump:speed"]  = { index = 3, type = "float32" },
            ["pump:status"] = { index = 1 },
        },
    },
}

local ws = WebsocketServer { port = 8080 }

pipe(plc, ws)   -- register changes → all WebSocket clients as JSON
pipe(ws, plc)   -- commands from clients → Modbus writes
```

The engine validates config, maps registers to named fields, and serializes messages.
`pipe` accepts any mix of workers and plain Lua functions; return a value to forward it downstream.

## Built-in workers

| Worker | What it does |
|---|---|
| `TcpModbusDevice` / `ModbusMaster` | Modbus TCP master with named register mapping |
| `WebsocketServer` / `WebsocketClient` | JSON or msgpack, broadcast or per-client routing |
| `RedisCache` / `RedisStream` | Hash key sync and stream append |
| `Sql` | SQLite / MySQL / PostgreSQL / ODBC |
| `Serial` | Serial port, optional SLIP framing + msgpack |
| `QML` | QML window as a bidirectional pipeline worker |
| `CanMaster` / `CyphalMaster` | CAN/Cyphal frame I/O |

## Lua API highlights

```lua
-- Message routing
pipe(a, b, c)            -- connect workers/functions in sequence
on(worker, "field", fn)  -- unwrap a field before the handler
wrap("key")              -- { v } → { key = { v } }
unwrap("key")            -- { key = { v } } → { v }
-- path syntax: "a:b:[2]"  or  "a/b/c" (custom separator)

-- Timers
after(1000, fn)          -- one-shot
each(500, fn)            -- repeating

-- Async/await across the Qt event loop
local rows, err = await(sql:Exec("SELECT * FROM users LIMIT 10"))

-- Request/response correlation
local call = make_service(req_worker, resp_worker, timeout_ms)
call({ amount = 100 }, function(res, err) ... end)

-- Custom workers in pure Lua
local my_worker = create_worker(function(self, msg)
    notify_all(self, transform(msg))
end)

-- Inline QML (no separate file needed)
local view = QML[[ Window { visible: true; ... } ]]
```

## Build

```bash
sudo apt install cmake ninja-build build-essential \
    libqt5websockets5-dev libqt5serialbus5-dev libqt5serialport5-dev \
    libqt5sql5-mysql libqt5sql5-odbc libqt5sql5-psql libqt5sql5-sqlite \
    qtdeclarative5-dev libqt5quickcontrols2-5

cmake -B build -G Ninja
cmake --build build -j $(nproc)

build/bin/radapter tests/smoke.lua   # smoke test (self-checking, no hardware needed)
```

Set `CPM_SOURCE_CACHE=$HOME/.cache/CPM` to cache dependencies across builds.

## Packaging (DEB)

Two DEB packages are produced from separate build configurations — `radapter-headless`
and `radapter-gui`. They conflict with each other: only one can be installed at a time.

```bash
# radapter-headless — no GUI dependencies
cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D RADAPTER_GUI=OFF \
    -D CMAKE_INSTALL_PREFIX=/usr -B build-headless
cmake --build build-headless -j $(nproc)
(cd build-headless && cpack -G DEB)

# radapter-gui — with QML/GUI support + QML module dependencies
cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D RADAPTER_GUI=ON \
    -D CMAKE_INSTALL_PREFIX=/usr -B build-gui
cmake --build build-gui -j $(nproc)
(cd build-gui && cpack -G DEB)
```

The packages install to `/usr/bin/radapter`, `/usr/lib/`, and `/usr/include/radapter/`.
Shared library dependencies are auto-detected by `dpkg-shlibdeps`; QML module dependencies
(`qml-module-qtquick2`, `qml-module-qtcharts`, etc.) are declared explicitly since they're
loaded dynamically at runtime.

Built packages:
- `build-headless/radapter-headless_3.0_amd64.deb`
- `build-gui/radapter-gui_3.0_amd64.deb`

## Running

```bash
build/bin/radapter script.lua                       # run a script
build/bin/radapter --schema                         # print JSON schema of all workers, then exit
build/bin/radapter --watch-dir . script.lua         # hot-reload on file change
build/bin/radapter --watch-dir . --pre-reload "cmake --build build" script.lua  # rebuild before each reload
build/bin/radapter --watch-dir . --pre-reload "cmake --build build" --reload-exec script.lua  # rebuild + restart (picks up embedded QML/scripts)
build/bin/radapter --gui script.lua                 # enable QML worker (exits when the last window closes)
build/bin/radapter --gui-no-auto-quit script.lua # keep running after the last window closes
build/bin/radapter -e 'log.info("hi")'              # inline eval
```

Arguments after the script file are available in Lua as `args`:

```bash
build/bin/radapter examples/serial/serial.lua /dev/ttyUSB0
```

## Architecture

radapter is a C++/Qt5 engine with a public SDK (`include/radapter/`). Three ways to extend it:

- **Lua scripts** — the normal path. Wire built-in workers, write transform functions, build pipelines.
- **Native plugins** — compile a separate `.so` against `radapter-sdk` and load at runtime with `load_plugin("path/to/lib.so")`. See `plugins/test.cpp` for the reference pattern.
- **Direct embedding** — link `radapter-sdk` into your own C++ executable and construct `radapter::Instance`.

Config structs are plain C++ structs annotated with `RAD_DESCRIBE`/`RAD_MEMBER`. The engine derives validation, conversion, and `--schema` output from that reflection automatically.

## LuaJIT

```bash
sudo apt install libluajit2-5.1-dev
cmake -B build -G Ninja -DRADAPTER_JIT=ON
cmake --build build -j $(nproc)
```

JIT mode targets Lua 5.1. Embedded scripts ship as source rather than bytecode.

## Examples

See `examples/` — runnable demonstrations (most need live hardware/services):

| Script | What to see |
|---|---|
| `examples/websocket.lua` | Server ↔ client with msgpack + zlib compression |
| `examples/redis.lua` | RedisCache + RedisStream + async/await |
| `examples/sql.lua` | SQLite insert/select with callbacks and `await` |
| `examples/modbus.lua` | Modbus TCP master (needs a device on :1502) |
| `examples/serial/serial.lua` | Serial + SLIP + msgpack (pass port as arg) |
| `examples/chat/` | Multi-client group chat: headless server + QML GUI client |
| `examples/demo/` | QML gauge + Redis + Serial + `make_service` request/response |
| `examples/test_async.lua` | `async`/`await`/`promisify` patterns |
| `examples/gui.lua` | Inline QML string, bidirectional color binding |
| `examples/modbus_table.lua` | `import radapter`'s `ModbusTable` — configurable live register table |
| `examples/can.lua` / `examples/cyphal.lua` | CAN/Cyphal (see `examples/setup_vcan.sh` for a virtual interface) |

## Tests

Self-checking scripts under `tests/` that the binary runs and exits 0/1 on:

| Script | What it verifies |
|---|---|
| `tests/smoke.lua` | Primary smoke test — every hardware-free worker, live roundtrips, naming |
| `tests/basic.lua` | Pipeline primitives, `wrap`/`unwrap`, `get`/`set` path syntax |
| `tests/modbus_loopback.lua` | Deep ModbusSlave ↔ ModbusMaster loopback |
| `tests/tags.lua` | Tag system (run with `--tags`) |

## GUI testing (offscreen)

The `QML_Tester` worker (available with `--gui`) lets you script QML UI interactions,
capture screenshots, and record/replay sessions — all headless under
`QT_QPA_PLATFORM=offscreen`, no display needed:

```bash
QT_QPA_PLATFORM=offscreen build/bin/radapter --gui -e '
local qt = QML_Tester()
local view = QML { url = "projects/scada/Configurator.qml",
    properties = { pickable_types = {}, initial_schemas = {} } }
qt:wait(500)
qt:screenshot("/tmp/configurator.png")
shutdown()
'
```

### Finding and inspecting items

Assign `objectName` in QML, then locate and inspect from Lua:

```lua
local qt = QML_Tester()
qt:windows()                              -- list open QML window titles
qt:set_window(0)                          -- select window by index (or title substring)
qt:find("submitBtn")                      -- find item by objectName; returns boolean
qt:find_all("regRow")                     -- all matching items
qt:prop("submitBtn", "text")              -- read a QML property
qt:prop("enabled")                        -- read property of the current item (last :find result)
qt:set_prop("label", "text", "OK")        -- set a property
local cx, cy = qt:center("btn")           -- get center coordinates in window space
```

### Emulating input

```lua
qt:click(200, 150)                        -- left-click at window coordinates
qt:click(200, 150, "right")               -- right-click
qt:dblclick(200, 150)                    -- double-click
qt:click_item("submitBtn")                -- click the center of a found item
qt:move(300, 200)                         -- mouse move
qt:wheel(200, 150, 120)                   -- scroll wheel (delta)
qt:key_click("Return")                   -- press + release a key
qt:key_click("A", "ctrl")                -- with modifiers ("shift","ctrl","alt","meta")
qt:key_press("Shift")                    -- press without releasing
qt:key_release("Shift")                  -- release a held key
qt:type("hello world")                   -- type a string one character at a time
```

Key names are case-insensitive: `"return"`, `"Return"`, `"RETURN"` all work. Supported names:
`"Return"`, `"Enter"`, `"Tab"`, `"Backspace"`, `"Space"`, `"Escape"`,
`"Left"`/`"Right"`/`"Up"`/`"Down"`, `"Home"`, `"End"`, `"PageUp"`/`"PageDown"`,
`"Insert"`, `"Delete"`, `"F1"`–`"F35"`, `"A"`–`"Z"`, `"0"`–`"9"`, `"Shift"`,
`"Control"`/`"Ctrl"`, `"Alt"`, `"Meta"`. Raw Qt key codes (integers) are also accepted.

### Screenshots

```lua
qt:screenshot("/tmp/result.png")          -- save window contents to PNG; returns boolean
```

### Record and replay

Record real user interactions (mouse clicks, moves, key presses, wheel events) to a JSON
file, then replay them later — useful for regression testing:

```lua
-- Record a session (user interacts manually while the script waits)
qt:record_start()
after(30000, function()
    local json = qt:record_stop("/tmp/session.json")
    shutdown()
end)

-- Replay at original speed, or 2x faster
qt:replay("/tmp/session.json")
qt:replay("/tmp/session.json", 2.0)

-- Replay from an inline JSON string
qt:replay_data(json_string, 3.0)
```

Replay injects the same Qt events (`QMouseEvent`, `QKeyEvent`, `QWheelEvent`) through
`QCoreApplication::sendEvent()` with the original inter-event delays (divided by the
speed multiplier). The recording format is a JSON array of `{t, type, x, y, btn, key,
mods, text, delta}` objects.

### Waiting

```lua
qt:wait(500)                              -- block and process events for N ms
qt:process_events()                       -- single non-blocking pass through the event loop
```

### Window and tab state persistence

The SCADA configurator saves window geometry, maximized/fullscreen state, screen
assignment, and tab layout across sessions via QSettings. This is gated on the global
context property `persistUi` (default `true`). Golden tests set `persistUi = false` to
prevent QSettings I/O from interfering with `QML_Tester` event replay.

## Special thanks

- smokie-l for inspiration
- [MobDebug](https://github.com/pkulchenko/MobDebug) for the remote debugger
