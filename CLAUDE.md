# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

radapter ("Redis Adapter") is **NodeJS-style plumbing software for industrial/embedded
integration**: it wires together a zoo of devices, protocols and databases and exposes them
to simple, usable **Lua** scripts. The C++/Qt5 engine provides *workers* (Modbus,
Websocket, Redis, SQL, Serial, CAN/Cyphal, GUI/QML); Lua scripts instantiate them, validate
their config, and connect them into message **pipelines**.

The architecture is deliberately **extensible two ways**:
1. **Direct linking** against `radapter-sdk` (the engine is a shared library with a public
   SDK in `include/radapter/`) — build your own executable or embed the `Instance`.
2. **Native plugins** — separate `.so`s that depend only on `radapter-sdk`, loaded at
   runtime from Lua via `load_plugin(...)`. See `plugins/test.cpp` for the reference.

## Build & run

```bash
# Configure + build (Ninja). Dependencies are fetched via CPM at configure time.
cmake -B build -G Ninja
cmake --build build -j $(nproc)

# Run a script
build/bin/radapter tests/basic.lua

# Print JSON schema of every registered worker/config, then exit
build/bin/radapter --schema

# Other useful flags
build/bin/radapter --watch-dir . examples/modbus.lua        # hot reload on file change
build/bin/radapter --gui --gui-auto-quit examples/chat/client.lua  # quit when window closes
build/bin/radapter --debug tests/basic.lua          # Mobdebug remote debugger :8172
build/bin/radapter --debug-vscode tests/basic.lua   # VSCode Mobdebug variant
build/bin/radapter -e 'log.info("hi")'              # eval inline; --gui enables QML
# trailing args after the script file land in the Lua global `args`
build/bin/radapter examples/serial/serial.lua /dev/ttyUSB0
```

Set `CPM_SOURCE_CACHE` (e.g. `export CPM_SOURCE_CACHE="$HOME/.cache/CPM"`) to cache fetched
packages across builds/clones.

System deps (Ubuntu): `cmake ninja-build build-essential` plus Qt5 dev packages
(`libqt5websockets5-dev libqt5serialbus5-dev libqt5serialport5-dev`, the `libqt5sql5-*`
drivers, `qtdeclarative5-dev libqt5quickcontrols2-5`). Full list in README.md.

### Key CMake options

- `RADAPTER_JIT` / `RADAPTER_JIT_STATIC` — use LuaJit instead of PUC-Rio Lua 5.4. JIT mode
  is **Lua 5.1 only**; embedded scripts ship as source instead of precompiled bytecode.
- `RADAPTER_GUI` (default ON) — enables Qt Gui/Qml/Quick and the `QML` worker.
- `RADAPTER_STATIC` — build the SDK static; **disables runtime plugins**.
- `RADAPTER_ROS2` — also build the out-of-tree ROS2 plugin under `plugins/ros/`.

### Adding CLI flags

`app/main.cpp` does a **pre-scan** of raw `argv` before argparse runs. This is necessary
**only for `--gui`**, because `QGuiApplication` must be constructed before argparse touches
`argc/argv`. All other flags must be added to the argparse parser and read after
`cli.parse_args(args)` — do not add new flags to the pre-scan loop.

### Tests

There is no unit-test framework. Self-checking scripts live under `tests/`; runnable
demonstrations live under `examples/`.

**Tests (`tests/`)** are self-checking — the binary runs them and they exit 0 on success /
1 on failure. **The primary smoke test is `tests/smoke.lua`** — run it after any engine
change (`build/bin/radapter tests/smoke.lua`); it constructs every worker that needs no
external hardware/services, verifies live roundtrips (websocket pairs, sqlite, a modbus
slave/master loopback, services, worker naming). `tests/modbus_loopback.lua` is a deeper
ModbusSlave <-> ModbusMaster test; `tests/basic.lua` covers the Lua builtins (pipe/get/set);
`tests/tags.lua` covers the tag system (run with `--tags`).

**Examples (`examples/`)** are documentation-grade demos; most need live hardware/services:
`modbus.lua` (a Modbus TCP device on :1502), `redis.lua` (a Redis server),
`serial/serial.lua` (a serial port arg), `can.lua`/`cyphal.lua` (a CAN interface — see
`examples/setup_vcan.sh` for a virtual one), `plugin.lua` (pass the plugins build dir),
`ros.lua` (ROS2 plugin). `examples/demo/` is a small QML dashboard example and
`examples/chat/` a headless server + QML client.

## Architecture

### Layers

- `app/main.cpp` → the `radapter` executable. Owns the `QCoreApplication`/`QGuiApplication`
  event loop, CLI parsing (argparse), signal handling (qctrl), and the `--watch-dir`
  hot-reload loop (which destroys the `Instance` and builds a fresh one on file change).
- `src/` → builds **`radapter-sdk`** (shared by default; `RADAPTER_API` = export/import).
  This is the engine: Lua VM wrapper, config/schema system, and all built-in workers.
- `include/radapter/` → the public SDK headers installed for SDK consumers and plugin
  authors (`radapter.hpp`, `worker.hpp`, `config.hpp`, `function.hpp`, `value.hpp`,
  `async_helpers.hpp`, `logs.hpp`).
- `plugins/` → independently-linked `.so`s depending only on `radapter-sdk`, built with the
  `create_radapter_plugin()` CMake helper. `plugins/test.cpp` is the reference;
  `plugins/ros/` is a full external-style plugin (own CMakeLists, finds rclcpp).

### Instance — the central object

`radapter::Instance` (`src/instance.cpp`, `src/instance_impl.hpp`) owns **one `lua_State`**
and the live set of `Worker`s. On construction it opens the Lua libs, registers the global
Lua API, loads the embedded scripts, then calls every entry in `builtin::workers::all`
(the `_all[]` array in `src/builtin.cpp`: `test, modbus, websocket, redis, sql, serial,
can, cyphal`) to register built-in workers. `Instance::FromLua(L)` recovers the instance
from any `lua_State` via a registry lightuserdata key. Shutdown is cooperative: it emits
`ShutdownRequest`, waits for each worker's `ShutdownDone` (or a timeout), then
`ShutdownDone`.

Embedded Lua scripts live in `src/scripts/*.lua`, are compiled to bytecode by the
`radapter-luac` host tool, and baked into a Qt resource (`:/scripts/...`) at build time —
except under JIT/cross builds, where they ship as source. `builtins.lua` defines the
pipeline primitives; `async.lua` provides coroutine-based promises (`await`);
`mobdebug*.lua` is the remote debugger; `socket.lua` is luasocket glue.

Reusable QML components live in `src/qml/*.qml` with a `qmldir` (module `radapter`). The
same `radapter_scripts` CMake function bakes them into the resource under `:/radapter/`,
and the GUI worker adds `qrc:/` to the QML import path, so scripts can `import radapter`
(e.g. `ModbusTable`). Data reaches a component by the GUI worker setting properties from
the incoming message (nested map fields recurse into child objects found by `objectName`);
a component that needs per-row reactive values creates persistent child holder objects
named after each field rather than routing into recycled list/table delegates.

### Workers and the message model

A `Worker` (`include/radapter/worker.hpp`) is a `QObject` with one inbound entry point and
outbound signals:

- inbound: `virtual void OnMsg(QVariant const& msg)` — a message arrived for this worker.
- outbound: `SendMsg` / `SendMsgField(key, v)` (data channel) and `SendEvent` /
  `SendEventField(key, v)` (event channel). The `*Field` variants wrap the value in a
  single-key map. In Lua, the data channel feeds the worker's normal listeners; the event
  channel feeds `worker.events` (see `radWorkerEvents` / `worker_index` in `src/worker.cpp`).

Messages are `QVariant` trees (maps/lists/scalars/`QByteArray`) — the same shape as JSON.
`src/builtin.hpp` defines `jv::Convert<QVariant>` bridging QVariant ↔ the `json_view`
representation used by binary protocols.

**Pipelines are pure Lua** (`src/scripts/builtins.lua`):
- `pipe(a, b, c)` subscribes each target to the previous one's listener list and returns
  the first. Plain Lua functions are auto-wrapped (`wrap_func`); their return value, if
  non-nil, is forwarded downstream.
- `on(worker, "field", handler)` unwraps a message field before the handler.
- `wrap(key)` / `unwrap(key)` build map-wrapping/unwrapping transformers (path syntax like
  `"a:b:[2]"`, custom separators supported — see `get`/`set` and `tests/basic.lua`).
- Calling a worker value like `ws{...}` sends it a message (`__call` → `OnMsg`).
- `make_service(request, response[, timeout])` builds request/response correlation by id
  on top of pipes.

### Registering a worker

Workers are bound to Lua globals by name:

```cpp
inst->RegisterWorker<MyWorker>("MyWorker", { {"Call", AsExtraMethod<&MyWorker::Call>} });
inst->RegisterSchema<MyConfig>("MyWorker");   // feeds --schema
```

A worker class must derive from `Worker` and be constructible from `(config, Instance*)`
(see `if_valid_worker`). `WorkerArguments` implicitly converts to any config type via
`ParseAs<T>`, so a constructor taking the config struct directly works; the first Lua call
argument becomes the config. `ExtraMethods` become callable methods on the Lua worker
object (`worker:Call(...)`); `AsExtraMethod<&Cls::m>` adapts a
`QVariant(Cls::*)(QVariantList const&)` member. Also available: `RegisterFunc` (plain Lua
function, e.g. `TcpModbusDevice` is a factory function not a worker) and `RegisterGlobal`.

In a plugin, use the `RADAPTER_PLUGIN(Name, "iid")` macro (`worker.hpp`); its body is the
`Initialize(Instance*, QVariantList)` implementation where you call `RegisterWorker`/etc.
Lua loads it with `load_plugin("path/to/lib")`.

### Config / schema system (`include/radapter/config.hpp`)

Config structs are plain structs annotated with the `describe` reflection library:
`RAD_DESCRIBE(Type)` + `RAD_MEMBER(field)` (enums use `RAD_ENUM` / `MEMBER`). Three parallel
template families drive everything off that reflection:

- `Parse(out, QVariant, frame)` — validate + convert incoming config. On mismatch it
  `Raise`s with a `TraceFrame` path (e.g. `ModbusMaster.registers.holding...`).
- `Dump(in, QVariant&)` — serialize back to a QVariant.
- `PopulateSchema(in, QVariant&)` — produce the human-readable `--schema` description.

Field wrappers: `WithDefault<T>` (has a default, schema shows `[has_default]`),
`std::optional<T>` / `OptionalPtr<T>` (`[optional]`), `vector<T>`, `map<K,T>`, tuples, and
nested described structs all compose recursively.

### Async, values, and Lua interop

- `LuaValue` / `LuaUserData` (`include/radapter/value.hpp`) are RAII handles to Lua
  registry refs. `LuaFunction` (`function.hpp`) wraps a callable Lua value: `Call(args)`
  and `CallAsync(args)` (returns a `fut::Future`).
- `async_helpers.hpp` bridges C++ `fut::Future`s to Lua: `makeLuaPromise` /
  `resolveLuaCallback` turn a future into a Lua callback-or-`await`able. Workers like Redis
  expose async ops this way (`await(cache:Exec("GET test"))` in `tests/redis.lua`).

## Conventions

- **`definitions.lua`** is the Lua LSP type stub for the entire radapter public API. Keep it
  up to date before committing: any new worker, extra method, global function, or worker
  field exposed to Lua must have a corresponding entry there.

- Comments: write no comments in C++ sources unless the WHY is non-obvious (hidden
  constraint, subtle invariant, workaround for a specific bug). Never describe what the code
  does — well-named identifiers do that. Lua scripts under `examples/` are public-facing
  documentation and may carry header comments and usage instructions.

- Logging: `worker->Info/Warn/Error/Debug("fmt {}", args)` (fmtlib syntax) or
  `inst->Info(category, ...)`. From Lua: `log.info(...)`, `log "msg"`, `log.set_level(...)`,
  `log.set_handler(fn)`.
- Raise errors in SDK/worker code with `radapter::Raise("msg {}", x)` (throws
  `std::runtime_error`, caught at the eval boundary) — don't return error codes.
- The SDK hides symbols by default (`-fvisibility=hidden`); anything crossing the
  shared-lib boundary must be marked `RADAPTER_API`.
- `src/**/*.cpp|*.hpp` are globbed with `CONFIGURE_DEPENDS`. Adding a built-in worker means:
  new dir under `src/workers/`, declare its `builtin::workers::<name>(Instance*)` init in
  `src/builtin.hpp`, and add it to `_all[]` in `src/builtin.cpp`.
- `src/workers/binary_worker.{hpp,cpp}` is a shared base for framed binary protocols (SLIP
  framing, msgpack, optional CRC) — reuse it for new serial/socket binary workers.

## Git workflow

After reaching a working functional checkpoint (e.g. a new config field added and at least
minimally exercised, a bug fixed and verified, a feature complete end-to-end), create a
commit automatically without waiting to be asked.

Commit message style: one short subject line (≤72 chars), optionally one small paragraph of
body if the motivation isn't obvious from the subject. Do not list minor incidental fixes
(typos, braces, accessor corrections) — only the meaningful change warrants mention. Err on
the side of terseness.
