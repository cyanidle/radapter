# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

radapter ("Redis Adapter") is **NodeJS-style plumbing software for industrial/embedded
integration**: it wires together a zoo of devices, protocols and databases and exposes them
to simple, usable **Lua** scripts. The C++/Qt6 engine provides *workers* (Modbus,
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
build/bin/radapter --watch-dir . examples/modbus/modbus.lua  # hot reload on file change
build/bin/radapter --gui examples/chat/client.lua  # quits when the last window closes
build/bin/radapter --gui-no-auto-quit examples/chat/client.lua  # keep running after the window closes
build/bin/radapter --gui --gui-record /tmp/out.json script.lua  # record all GUI interactions
build/bin/radapter --debug tests/basic.lua          # Mobdebug remote debugger :8172
build/bin/radapter --debug-vscode tests/basic.lua   # VSCode Mobdebug variant
build/bin/radapter -e 'log.info("hi")'              # eval inline; --gui enables QML
# trailing args after the script file land in the Lua global `args`
build/bin/radapter examples/serial/serial.lua /dev/ttyUSB0
```

Set `CPM_SOURCE_CACHE` (e.g. `export CPM_SOURCE_CACHE="$HOME/.cache/CPM"`) to cache fetched
packages across builds/clones.

System deps (Ubuntu): `cmake ninja-build build-essential` plus Qt6 dev packages
(`qt6-base-dev qt6-websockets-dev qt6-serialbus-dev qt6-serialport-dev`, the `libqt6sql6-*`
drivers, `qt6-declarative-dev qml6-module-qtquick-controls`). Full list in README.md.

### DEB packaging

Two mutually-conflicting DEB packages (`radapter-gui` / `radapter-headless`) are built
from separate configurations (`RADAPTER_GUI=ON` vs `OFF`) via CPack:

```bash
cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D RADAPTER_GUI=OFF -D CMAKE_INSTALL_PREFIX=/usr -B build-headless
cmake --build build-headless -j $(nproc) && (cd build-headless && cpack -G DEB)

cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D RADAPTER_GUI=ON -D CMAKE_INSTALL_PREFIX=/usr -B build-gui
cmake --build build-gui -j $(nproc) && (cd build-gui && cpack -G DEB)
```

QML module deps (invisible to dpkg-shlibdeps) are declared explicitly in CMakeLists.txt
via CPACK_DEBIAN_PACKAGE_DEPENDS. When adding new QML imports, update the
RADAPTER_DEB_GUI_QML_DEPS list.

### Key CMake options

- `RADAPTER_JIT` / `RADAPTER_JIT_STATIC` — use LuaJit instead of PUC-Rio Lua 5.4. JIT mode
  is **Lua 5.1 only**; embedded scripts ship as source instead of precompiled bytecode.
- `RADAPTER_GUI` (default ON) — enables Qt Gui/Qml/Quick and the `QML` worker.
- `RADAPTER_STATIC` — build the SDK static; **disables runtime plugins**.
- `RADAPTER_ROS2` — also build the out-of-tree ROS2 plugin under `plugins/ros/`.

### Adding CLI flags

`app/main.cpp` does a **pre-scan** of raw `argv` before argparse runs. This is necessary
**only for the GUI-enabling flags** (`--gui`, and `--gui-no-auto-quit` which also implies it),
because `QGuiApplication` must be constructed before argparse touches `argc/argv`. All
other flags must be added to the argparse parser and read after `cli.parse_args(args)` —
do not add new flags to the pre-scan loop.

### Tests

There is no unit-test framework. Self-checking scripts live under `tests/`; runnable
demonstrations live under `examples/`.

**Tests (`tests/`)** are self-checking — the binary runs them and they exit 0 on success /
1 on failure. **`tests/all.lua` runs the whole suite** — it spawns every other
`tests/*.lua` in its own radapter process (via the `Process` worker, so they can't clash on
ports/sockets/shutdown), applies any needed flags, and exits 0 iff all pass
(`build/bin/radapter tests/all.lua`); new tests are auto-discovered. **The primary
smoke test is `tests/smoke.lua`** — run it after any engine change
(`build/bin/radapter tests/smoke.lua`); it constructs every worker that needs no external
hardware/services, verifies live roundtrips (websocket pairs, sqlite, a modbus slave/master
loopback, services, worker naming). `tests/modbus_loopback.lua` is a deeper ModbusSlave <->
ModbusMaster test; `tests/basic.lua` covers the Lua builtins (pipe/get/set);
`tests/local.lua` the local-IPC workers; `projects/scada/scada.lua test` runs the golden test suite;
`tests/tags.lua` the tag system (run with `--tags`).

After making some changes **ALWAYS** check result somehow

When the engine code (src/, include/) is touched, always run `build/bin/radapter tests/all.lua`.
When the project (projects/) touches QML or Lua config files, always launch QML **headlessly**
to verify the visual components (write short test as inline -e "code" for example, dont forget to shutdown() at the end)

When writing a short test/`-e` snippet, always end it with `shutdown()` — otherwise the
event loop keeps running and the process hangs instead of exiting on success.

#### Offscreen visual verification with screenshots

For QML changes, prefer capturing a screenshot to verify rendering. Use `QT_QPA_PLATFORM=offscreen`
and the `QML_Tester` worker (available under `--gui`) to script interactions and capture
screenshots directly from Lua — no QML-side hooks needed:

```bash
QT_QPA_PLATFORM=offscreen build/bin/radapter --gui -e '
local qt = QML_Tester()
local view = QML { url = "projects/scada/configurator/Configurator.qml",
    properties = { pickable_types = {}, initial_schemas = {} } }
qt:wait(500)
qt:screenshot("/tmp/radapter_smoke.png")
shutdown()
'
```

After the test, inspect with `Read /tmp/radapter_smoke.png` — Claude Code can display PNGs
inline, making it easy to spot rendering regressions (blank areas, layout collapse, etc.).

##### Emulating UI input with `QML_Tester`

The `QML_Tester` worker can find QML items by `objectName`, read/write their properties,
inject mouse/keyboard events, and record/replay interaction sequences:

```lua
local qt = QML_Tester()
qt:wait(500)                            -- let the window render
qt:windows()                            -- list open QML windows
qt:find("submitBtn")                    -- locate an item by objectName
qt:prop("submitBtn", "text")            -- read a QML property
qt:set_prop("label", "text", "Done")    -- set a property
qt:click_item("submitBtn")              -- click center of an item
qt:click(200, 150)                     -- click at window coordinates
qt:key_click("Return")                 -- press a key
qt:type("hello")                       -- type text into focused control
qt:screenshot("/tmp/out.png")          -- capture window to PNG

-- Record a sequence of interactions, then replay it at 2x speed
qt:record_start()
qt:click(100, 200)
qt:key_click("Tab")
qt:replay_data(qt:record_stop("/tmp/rec.json"), 2.0)
```

Keys are named (`"Return"`, `"Tab"`, `"Escape"`, `"A"`, `"F1"`, …) or raw Qt key codes.
Key names are case-insensitive. Optional modifiers: `qt:key_click("A", "ctrl")` for Ctrl+A.
Mouse buttons: `qt:click(x, y, "right")` / `"middle"` (default `"left"`).

To make an item findable, set `objectName` on it in QML:
```qml
Button { objectName: "submitBtn"; text: "Submit" }
```

The tool records timestamps, coordinates, key codes, and button types. Replay injects the
same Qt events with the original delays (divided by the speed multiplier). Recording
captures real user input (mouse/key events via an event filter on the QQuickWindow);
replay feeds them back through `QCoreApplication::sendEvent()`.

##### Serious QML testing workflow

After making QML changes, verify them systematically — don't just eyeball the code:

1. **Screenshot the relevant state.** Use `QT_QPA_PLATFORM=offscreen` with `QML_Tester`:
   ```bash
   QT_QPA_PLATFORM=offscreen build/bin/radapter --gui -e '
   local qt = QML_Tester()
   QML { url = "projects/scada/configurator/Configurator.qml", properties = { ... } }
   qt:wait(500)
   qt:screenshot("/tmp/before.png")
   shutdown()
   '
   ```
   Inspect with `Read /tmp/before.png` — blank areas, layout collapse, clipped text are
   immediately visible.

2. **Emulate interactions and capture the result.** Script clicks, key presses, and text
   entry; screenshot after each step:
   ```lua
   qt:click_item("addWorkerBtn")
   qt:wait(300)
   qt:click_item("ModbusMaster")
   qt:wait(300)
   qt:screenshot("/tmp/after_add.png")
   ```

3. **Record/replay for rapid iteration.** Record a sequence once, replay it at speed:
   ```lua
   qt:record_start()
   -- … interact manually or via script …
   local json = qt:record_stop("/tmp/seq.json")
   -- Then replay at 2x speed:
   qt:replay("/tmp/seq.json", 2.0)
   ```
   Use `replay_data` to embed the JSON inline in a test snippet.

4. **Use `--gui-record` to capture full sessions.** Launch with `--gui-record <path>` to
   automatically record every GUI interaction from startup to shutdown (path is mandatory):
   ```bash
   build/bin/radapter --gui --gui-record /tmp/session.json script.lua
   ```
   The recording is saved on exit (normal shutdown, Ctrl+C, reload, or last-window-close).

   From QML, **`radapter.note("msg")`** interleaves a `{type:"note", t, msg}` marker into
   the active `--gui-record` stream (a noop when no recording is running — it does *not*
   log). Use it to annotate what the UI is doing at a given moment while diagnosing a
   timing bug: sprinkle `radapter.note(...)` in the suspect QML, record with `--gui-record`,
   then read the JSON to see the notes in event order. Replay ignores `note` entries.

5. **For difficult bugs, ask the user to record a reproduction.** If a QML bug resists
   scripting (complex drag-and-drop, timing-dependent behavior, a mystery interaction),
   ask: *"Could you record a JSON reproduction with `--gui-record /tmp/bug.json` and share
   it?"* The recording captures exact mouse/key/wheel events with timestamps — replay it
   verbatim, or inspect the JSON to understand the trigger sequence.

Always run tests with some timeout provided to avoid hanging

**Examples (`examples/`)** are documentation-grade demos; most need live hardware/services:
`modbus/modbus.lua` (a Modbus TCP device on :1502) and `modbus/modbus_table.lua` (a GUI
table, self-contained loopback), `redis.lua` (a Redis server), `serial/serial.lua` (a
serial port arg), `can.lua`/`cyphal.lua` (a CAN interface — see `examples/setup_vcan.sh`
for a virtual one), `plugin.lua` (pass the plugins build dir), `ros.lua` (ROS2 plugin).
`examples/demo/` is a small QML dashboard example and `examples/chat/` a headless server +
QML client. **Projects (`projects/`)** are full apps built on radapter: `projects/scada/`
is the schema-driven configurator (`--gui projects/scada/scada.lua config`).

### UI state persistence

`WindowSettings.qml` (in `projects/scada/configurator/`) persists an `ApplicationWindow`'s geometry,
maximized/fullscreen state, and screen assignment across sessions via `Qt.labs.settings`
(QSettings). `DetachableTabs.qml` persists its tab-group layout and detached-panel state.
Both are gated on the **global context property `persistUi`** — set from Lua in the `QML{}`
call's `properties` table:

- **Production** (`scada.lua config`): `persistUi = true` → saves to `~/.config/radapter/radapter.conf`
- **Production, no persistence** (pass `ui-no-persist` as trailing arg to `scada.lua config`): `persistUi = false`
- **Golden tests** (`scada.lua test`): `persistUi = false` → no QSettings I/O, so
  `QML_Tester` event recording/replay is not disrupted
- **Absent**: defaults to `true` (e.g. `scada.lua run` opening the HMI window)

When **recording a new golden**, pass `ui-no-persist` so the recorded events
don't include QSettings I/O side effects:
```bash
build/bin/radapter --gui --gui-record projects/scada/goldens/new.json \
    projects/scada/scada.lua config ui-no-persist
```

To disable persistence when writing ad-hoc QML test snippets, pass `persistUi = false`:
```lua
QML { url = "...", properties = { persistUi = false, ... } }
```

## Architecture

### Layers

- `app/main.cpp` → the `radapter` executable. Owns the event loop
  (`QCoreApplication` headless; `QApplication` under `--gui` — a widget-capable app is
  required because QtCharts' QML module renders through QtWidgets' `QGraphicsScene`),
  CLI parsing (argparse), signal handling (qctrl), and the `--watch-dir`
  hot-reload loop (which destroys the `Instance` and builds a fresh one on file change).
  `--pre-reload "<cmd>"` runs a shell command (e.g. `cmake --build build`) before each
  reload and skips the reload — keeping the current instance live — if the command fails.
  `--reload-exec` makes reload `execv` the (rebuilt) binary instead of rebuilding the
  `Instance` in-process, so changes baked into the executable (embedded QML/scripts) take
  effect; POSIX only, falls back to in-process reload elsewhere. A reload-in-flight guard
  ignores file events caused by the pre-reload command's own side effects (e.g. build
  output landing in a watched dir).
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
can, cyphal, process, local, http`) to register built-in workers. `Instance::FromLua(L)` recovers the instance
from any `lua_State` via a registry lightuserdata key. Shutdown is cooperative: it emits
`ShutdownRequest`, waits for each worker's `ShutdownDone` (or a timeout), then
`ShutdownDone`.

Only **fundamental** Lua scripts are embedded: they live in `src/scripts/*.lua`, are
compiled to bytecode by the `radapter-luac` host tool, and baked into a Qt resource
(`:/scripts/...`) at build time — except under JIT/cross builds, where they ship as source.
`builtins.lua` defines the pipeline primitives; `async.lua` provides coroutine-based
promises (`await`); `mobdebug*.lua` is the remote debugger; `socket.lua` is luasocket glue.
Non-fundamental modules (e.g. `declare`) are **not** embedded — they live next to the
project that uses them and are `require`d from disk: `EvalFile` prepends the running
script's directory to `package.path`, so a script resolves sibling modules.

**No QML is embedded.** Reusable QML components live with the project/example that uses
them and resolve by **same-directory** lookup — sibling `.qml` files are usable as types
without any import (no `qmldir`, no `qrc:/`). `projects/scada/` holds the schema-driven
configurator (its QML + the `declare`/`runner` Lua); `examples/modbus/` holds `ModbusTable`
and the Modbus demos. Load a project's window from Lua with `QML { url = "./Foo.qml" }`.

The GUI worker (`src/workers/gui/gui.cpp`) exposes a `radapter` context object whose
`radapter.model` is the root of a tree of `GuiModel` nodes (a `QQmlPropertyMap` subclass).
Both data channels map onto the worker's `OnMsg`/`SendMsg` and are **scoped to a node's path
in the tree** (the root node's path is empty → flat messages):
- **State** — `model.node("regs")` gets/creates a nested node; `model[key]` is a reactive
  value (use `ensure(key)` to make a key bindable before data arrives). Inbound messages are
  merged in via `applyIncoming` (C++ `insert`, so no echo); a QML write to a key goes through
  `updateValue`, which auto-emits the change wrapped in the node's path (a write to the `regs`
  node's `enable` emits `{regs={enable=..}}`) — symmetric with `pipe(src, wrap("regs"), view)`
  / `pipe(view, unwrap("regs"), src)`. Bind `model[key]` and let edits emit automatically (no
  manual send). `ModbusTable` takes its node via `model:` and binds `model[name]` per row —
  no holders, no delegate routing.
- **Events** — `node.send(msg)` (out) and the `node.received(msg)` signal (in), for
  streams/RPC that aren't state (chat log, request/response). `send` wraps `msg` in the
  node's path and `received` fires on each node with the inbound sub-message scoped to it, so
  a nested component is addressed correctly without a manual prefix. `radapter.model.send` /
  `radapter.model.received` (root node) are the flat/global form. (It's `send`, not `emit` —
  `emit` is a reserved Qt macro and can't be a method name.)

A two-way control (CheckBox/TextField/Slider) bound to external state must use a `Binding`
element (or imperative set on change) for its display, because a user interaction
imperatively writes the control's property and **breaks any inline binding** on it — see
`ModbusTable`'s checkbox.

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
- **Fail fast; don't silence the unexpected.** Don't add defensive `if (x == null) …`
  guards or `x || default` fallbacks for conditions that shouldn't happen — they hide bugs
  behind plausible-looking output. Let it crash, or assert/`Raise` with a clear message.
  Only guard inputs that are *legitimately* optional, and then handle them explicitly
  (not by coalescing to a silent default). This applies to Lua/QML too (e.g. don't paper
  over an `undefined` that means a caller passed the wrong thing).
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
