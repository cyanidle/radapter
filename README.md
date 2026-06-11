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

## Running

```bash
build/bin/radapter script.lua                       # run a script
build/bin/radapter --schema                         # print JSON schema of all workers, then exit
build/bin/radapter --watch-dir . script.lua         # hot-reload on file change
build/bin/radapter --gui script.lua                 # enable QML worker
build/bin/radapter --gui --gui-auto-quit script.lua # exit when the window closes
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
| `examples/can.lua` / `examples/cyphal.lua` | CAN/Cyphal (see `examples/setup_vcan.sh` for a virtual interface) |

## Tests

Self-checking scripts under `tests/` that the binary runs and exits 0/1 on:

| Script | What it verifies |
|---|---|
| `tests/smoke.lua` | Primary smoke test — every hardware-free worker, live roundtrips, naming |
| `tests/basic.lua` | Pipeline primitives, `wrap`/`unwrap`, `get`/`set` path syntax |
| `tests/modbus_loopback.lua` | Deep ModbusSlave ↔ ModbusMaster loopback |
| `tests/tags.lua` | Tag system (run with `--tags`) |

## Special thanks

- smokie-l for inspiration
- [MobDebug](https://github.com/pkulchenko/MobDebug) for the remote debugger
