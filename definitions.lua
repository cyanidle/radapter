---@meta radapter

---@param path string
function load_plugin(path, ...) end

---Config schema of registered workers (as printed by `--schema`). No args -> a
---map of every worker; one name -> that worker's schema (or nil); several names
----> a map of just those.
---@param ... string
---@return table
function schema(...) end

---@param table table
---@param key string
---@param sep string?
---@return any
function get(table, key, sep) end

---@param table table
---@param key string
---@param value any
---@param sep string?
---@return table
function set(table, key, value, sep) end

---Register a callback to be called during shutdown, before workers are destroyed.
---Multiple callbacks accumulate; each is called in registration order.
---@param handler fun()
function on_shutdown(handler) end

---@param timeout number?
function shutdown(timeout) end

---@param key string
---@param sep string?
---@return fun(object: any): any
function wrap(key, sep) end

---@param key string
---@param sep string?
---@return fun(object: any): any
function unwrap(key, sep) end

---@param table any[]
function call_all(table, ...) end

-- TODO: docs regarding Service API
---@param request Worker
---@param responce Worker
---@param timeout number?
---@return fun(req: any, timeout: number?): asyncThunk
function make_service(request, responce, timeout) end

-- Declarative config (the `declare` module, required as `require "declare"`).

---A reference to another entry in the `objects` map, resolved at build time.
---@class RadRef
---@field ref string

---A buildable object: a worker or shared device.
---`config` may contain RadRef values (e.g. a Modbus master's `device`).
---@class RadObjectEntry
---@field type string -- name of the global factory (e.g. "ModbusMaster", "TcpModbusDevice")
---@field config table

---A connection between two objects. Optional transform: at most one of
---wrap/unwrap/on (a field key). With none, it is a direct pipe.
---@class RadPipe
---@field from string
---@field to string
---@field wrap string?
---@field unwrap string?
---@field on string?

---A node in the operator visualization (HMI) tree. Either a layout container
---(type Row/Column/Grid, with `children`) or a widget leaf (type Gauge/InfoDisplay/
---Spacer/Custom, bound to a `tag`). See projects/scada/hmi/Node.qml.
---@class RadVizNode
---@field type string -- "Row"|"Column"|"Grid"|"Gauge"|"InfoDisplay"|"Spacer"|"Custom"
---@field children RadVizNode[]? -- container children
---@field tag string? -- "<worker>:<field>" tag the leaf binds to
---@field source string? -- Custom: path/URL of a .qml file to load
---@field spacing number? -- container child spacing
---@field columns number? -- Grid column count
---@field min number? -- Gauge range minimum
---@field max number? -- Gauge range maximum
---@field label string? -- widget caption
---@field units string? -- value units suffix
---@field color string? -- Gauge arc color
---@field fillWidth boolean? -- Layout.fillWidth hint
---@field fillHeight boolean? -- Layout.fillHeight hint
---@field preferredWidth number? -- Layout.preferredWidth hint
---@field preferredHeight number? -- Layout.preferredHeight hint

---@class RadVisualization
---@field root RadVizNode

---@class RadConfig
---@field objects table<string, RadObjectEntry>
---@field pipes RadPipe[]?
---@field visualization RadVisualization? -- operator HMI authored by the scada configurator

---@class RadSaveParams
---@field config RadConfig
---@field path string? -- write whole config as JSON to this file
---@field key string? -- write config as a Redis hash under this key (one field per object)
---@field host string?
---@field port number?
---@field db number?

---@class RadLoadParams
---@field path string? -- read config JSON from this file
---@field key string? -- read config from a Redis hash under this key
---@field host string?
---@field port number?
---@field db number?

---@class radapterDeclare
local declare = {}

---Instantiate every object and wire the declared pipes.
---@param config RadConfig
---@return table<string, Worker>
function declare.build(config) end

---Serialize a config to a file (`path`) and/or a Redis hash (`key`).
---The Redis path is async: call at script top level or inside `async(...)`.
---@param params RadSaveParams
function declare.save_to(params) end

---Read a config (file `path` or Redis `key`) and build it.
---The Redis path is async: call at script top level or inside `async(...)`.
---@param params RadLoadParams
---@return table<string, Worker>
function declare.load_from(params) end

---@enum (key) loggingLevel
loggingLevel = {
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
}


---@class lfsAttrs
---@field dev string
---@field ino number 
---@field mode "file" | "directory" | "link" | "socket" | "named pipe" | "char device" | "block device" | "other"
---@field nlink any --number of hard links to the file
---@field uid any --user-id of owner (Unix only, always 0 on Windows)
---@field gid any --group-id of owner (Unix only, always 0 on Windows)
---@field rdev any --on Unix systems, represents the device type, for special file inodes. On Windows systems represents the same as dev
---@field access any --time of last access
---@field modification any --time of last data modification
---@field change any --time of last file status change
---@field size any --file size, in bytes
---@field permissions any --file permissions string
---@field blocks any --block allocated for file; (Unix only)
---@field blksize any --optimal file system I/O blocksize; (Unix only)

---@class lfs
lfs = {}

---@param filepath string
---@param request_name string?
---@return lfsAttrs, string?, number?
function lfs.attributes(filepath, request_name) end

---@param filepath string
---@param result_table table
---@return nil
function lfs.attributes(filepath, result_table) end

---@param path string
function lfs.chdir(path) end
    
---@param path string
function lfs.dir(path) end


--Creates a lockfile (called lockfile.lfs) in path if it does not exist and returns the lock. If the lock already exists checks if it's stale, using the second parameter (default for the second parameter is INT_MAX, which in practice means the lock will never be stale. To free the the lock call lock:free().
--In case of any errors it returns nil and the error message. In particular, if the lock exists and is not stale it returns the "File exists" message.
---@param path string
---@param seconds_stale number?
function lfs.lock_dir(path, seconds_stale) end

--Returns a string with the current working directory or nil plus an error string.
---@return string
function lfs.currentdir () end

--Lua iterator over the entries of a given directory. Each time the iterator is called with dir_obj it returns a directory entry's name as a string, or nil if there are no more entries. You can also iterate by calling dir_obj:next(), and explicitly close the directory before the iteration finished with dir_obj:close(). Raises an error if path is not a directory. 
---@param path string
---@return any, any
function lfs.dir (path) end -- -> iter, dir_obj


--Locks a file or a part of it. This function works on open files; the file handle should be specified as the first argument. The string mode could be either r (for a read/shared lock) or w (for a write/exclusive lock). The optional arguments start and length can be used to specify a starting point and its length; both should be numbers.
--Returns true if the operation was successful; in case of error, it returns nil plus an error string. 
---@param filehandle string
---@param mode any?
---@param start any?
---@param length any?
function lfs.lock (filehandle, mode, start, length) end

--Creates a link. The first argument is the object to link to and the second is the name of the link. If the optional third argument is true, the link will by a symbolic link (by default, a hard link is created). 
function lfs.link (old, new, symlink) end

--Creates a new directory. The argument is the name of the new directory.
--Returns true in case of success or nil, an error message and a system-dependent error code in case of error. 
function lfs.mkdir (dirname) end

--Removes an existing directory. The argument is the name of the directory.
--Returns true in case of success or nil, an error message and a system-dependent error code in case of error. 
---@param dirname string
---@return true, string?, number?
function lfs.rmdir (dirname) end

--Sets the writing mode for a file. The mode string can be either "binary" or "text". 
-- Returns true followed the previous mode string for the file, or nil followed by an error string in case of errors. 
-- On non-Windows platforms, where the two modes are identical, setting the mode has no effect, and the mode is always returned as binary. 
---@param file string
---@param mode "binary" | "text"
function lfs.setmode (file, mode) end

--Identical to attributes except that it obtains information about the link itself (not the file it refers to). It also adds a target field, containing the file name that the symlink points to. 
--On Windows this function does not yet support links, and is identical to attributes. 
function lfs.symlinkattributes (filepath, request_name) end

--Set access and modification times of a file. This function is a bind to utime function. The first argument is the filename, the second argument (atime) is the access time, and the third argument (mtime) is the modification time. Both times are provided in seconds (which should be generated with Lua standard function os.time). If the modification time is omitted, the access time provided is used; if both times are omitted, the current time is used.
--Returns true in case of success or nil, an error message and a system-dependent error code in case of error. 
function lfs.touch (filepath, atime, mtime) end

--Unlocks a file or a part of it. This function works on open files; the file handle should be specified as the first argument. 
--The optional arguments start and length can be used to specify a starting point and its length; both should be numbers.
-- Returns true if the operation was successful; in case of error, it returns nil plus an error string. 
function lfs.unlock (filehandle, start, length) end



---@class loggingMsg
---@field level loggingLevel
---@field msg string
---@field category string
---@field timestamp number

---@param fmt string
---@return string
function fmt(fmt, ...) end

---@param data any
---@return string
function json_encode(data) end

---@param json string
---@return any
function json_decode(json) end

---Return a process-unique, monotonically increasing integer id.
---@return integer
function next_id() end

---@class promise<T>: { __call: fun(self: promise<T>, callback: fun(result: T, err: string)) }

---@alias asyncThunk<TIn, TOut> fun(input: TIn): promise<TOut>

---@generic T
---@param fn fun(...): T
---@return fun(...): promise<T>
function async(fn) end

---@generic T
---@param p promise<T>
---@return T
function await(p) end

---@param fn fun(...) wrapped function whose last arg is a callback(result, err)
---@return fun(...): promise<any>
function promisify(fn) end

---@class logging
---@overload fun(msg: string)
---@overload fun(fmt: string, ...)
log = {
    ---@param fmt string
    info = function (fmt, ...) end,
    ---@param fmt string
    warn = function (fmt, ...) end,
    ---@param fmt string
    error = function (fmt, ...) end,
    ---@param fmt string
    debug = function (fmt, ...) end,
 
    ---@param handler fun(msg: loggingMsg)?
    set_handler = function (handler) end,

    ---@param level loggingLevel
    ---@overload fun(category: string, level: loggingLevel)
    set_level = function (level) end,
}

---@alias MsgHandler fun(msg: any, source: Worker): any
---@alias MsgHandlerEx fun(self: Worker, msg: any, source: Worker): any

---@class Events
---@field get_listeners fun(self: Pipable): MsgHandler[]

---@class Pipable: Events
---@field call MsgHandlerEx


---@class Worker: Pipable
---@overload fun(msg: any, source: Worker)
---@field events Events
---@field name string auto-generated or explicitly configured unique worker name
---@field origin string Lua "file:line" where the worker was created, or "<CPP>"
---@field destroy fun(self: Worker) synchronously stop and delete the worker
---@field shutdown fun(self: Worker): promise<nil> asynchronously stop the worker; await it to know when it has finished

---@alias pipeInput (Events | MsgHandler)

---@generic T1 : pipeInput
---@param first T1
---@vararg pipeInput
---@return T1
function pipe(first, ...) end

---@generic T1 : pipeInput
---@param source T1
---@param part string
---@param handler pipeInput
---@return T1
function on(source, part, handler) end

---@param worker Worker
---@param msg any
---@param sender Worker?
function notify_all(worker, msg, sender) end

---@param on_msg MsgHandlerEx
---@return Pipable
function create_worker(on_msg) end

---Look up a live worker by its unique name; nil if none exists.
---@type table<string, Worker?>
workers = {}

---@type string[]
args = {}

---@class builtinTimer
builtinTimer = {}
function builtinTimer:Stop() end

---@param timeout number
---@param callback fun(): any
---@return builtinTimer
function after(timeout, callback) end

---@param timeout number
---@param callback fun(): any
---@return builtinTimer
function each(timeout, callback) end

---@class TestWorker: Worker
---@field Call fun(self: TestWorker, callback: fun(a: number, b: number, c: number)): nil

---@return TestWorker
function TestWorker (params) end

---@class SqlWorker: Worker
SqlWorker = {}

---@alias SqlCallback fun(result: any[][], error: string)

---@param statement string
---@param callback SqlCallback
---@return nil
function SqlWorker:Exec(statement, callback) end

---@param statement string
---@param params table
---@param callback SqlCallback
---@return nil
function SqlWorker:Exec(statement, params, callback) end

---@param statement string
---@return fun(defer: SqlCallback)
function SqlWorker:Exec(statement) end

---@param statement string
---@param params table
---@return fun(defer: SqlCallback)
function SqlWorker:Exec(statement, params) end

---@return SqlWorker
function Sql (params) end


---@class ModbusMasterDevice

---@class ModbusSlaveDevice

---@class WorkerBaseParams
---@field name string? explicit unique worker name (auto-generated if omitted)
---@field category string? log category override

---@class ModbusMasterParams: WorkerBaseParams
---@field device ModbusMasterDevice
---@field slave_id number
---@field poll_rate number? milliseconds between polls (default 500)
---@field response_time number? ms to wait for response (default 150)
---@field write_retries number? (default 3)
---@field registers ModbusRegistersMap

---@class ModbusSlaveParams: WorkerBaseParams
---@field device ModbusSlaveDevice
---@field slave_id number
---@field registers ModbusRegistersMap

---@class ModbusRegister
---@field index number 0-based register index
---@field type ("uint16"|"uint32"|"float32")?
---@field mode ("r"|"rw"|"w")?

---@class ModbusRegistersMap
---@field holding table<string, ModbusRegister>?
---@field coils table<string, ModbusRegister>?
---@field di table<string, ModbusRegister>?
---@field input table<string, ModbusRegister>?

---@return Worker
---@param params ModbusMasterParams
function ModbusMaster(params) end

---@return Worker
---@param params ModbusSlaveParams
function ModbusSlave(params) end

---@return ModbusMasterDevice
function TcpModbusDevice(params) end
---@return ModbusMasterDevice
function RtuModbusDevice(params) end

---@return ModbusSlaveDevice
function TcpModbusServer(params) end
---@return ModbusSlaveDevice
function RtuModbusServer(params) end

---@class QMLWorker: Worker
QMLWorker = {}

---@return string
function QMLWorker:dir() end

---@class QMLParams
---@field url string

---@return QMLWorker
---@param params QMLParams
function QML(params) end;

---@return QMLWorker
---@param qmlSource string
function QML(qmlSource) end;

-- QML UI test worker (available only with --gui).  Construct with no arguments.
---@class QML_Tester : Worker
QML_Tester = {}

---Process events for `ms` milliseconds, blocking.
---@param ms integer
function QML_Tester:wait(ms) end

---Process all pending events (single pass, non-blocking).
function QML_Tester:process_events() end

---Select the current window by index (0-based) or title substring.
---@param idx_or_title integer|string
function QML_Tester:set_window(idx_or_title) end

---Return a list of window titles (1-based Lua array).
---@return string[]
function QML_Tester:windows() end

---Find an item by objectName in the current window. Sets it as current item.
---@param objectName string
---@return boolean true if found
function QML_Tester:find(objectName) end

---Return the objectNames of all items matching `objectName` in the current window.
---@param objectName string
---@return string[]
function QML_Tester:find_all(objectName) end

---Read a property of the current item, or of a named item.
---@overload fun(self: QML_Tester, propName: string): any
---@overload fun(self: QML_Tester, itemName: string, propName: string): any
function QML_Tester:prop(itemName, propName) end

---Set a property on the current item, or on a named item.
---@overload fun(self: QML_Tester, propName: string, value: any)
---@overload fun(self: QML_Tester, itemName: string, propName: string, value: any)
function QML_Tester:set_prop(itemName, propName, value) end

---Return the center (x, y) of the current item in window coordinates.
---@overload fun(self: QML_Tester): number, number
---@overload fun(self: QML_Tester, itemName: string): number, number
function QML_Tester:center(itemName) end

---Left-click at window coordinates (x, y).  Optional button: "left" (default), "right", "middle".
---@param x number
---@param y number
---@param button? string
function QML_Tester:click(x, y, button) end

---Double-click at window coordinates (x, y).
---@param x number
---@param y number
---@param button? string
function QML_Tester:dblclick(x, y, button) end

---Mouse press (without release) at (x, y).
---@param x number
---@param y number
---@param button? string
function QML_Tester:press(x, y, button) end

---Mouse release at (x, y).
---@param x number
---@param y number
---@param button? string
function QML_Tester:release(x, y, button) end

---Move the mouse to (x, y).
---@param x number
---@param y number
function QML_Tester:move(x, y) end

---Scroll wheel at (x, y) by delta (positive = up).
---@param x number
---@param y number
---@param delta integer
function QML_Tester:wheel(x, y, delta) end

---Click the center of the current item, or of a named item.
---@param objectName? string
function QML_Tester:click_item(objectName) end

---Press and release a key.  Optional modifiers: "shift", "ctrl", "alt", "meta".
---Key names are case-insensitive ("return", "RETURN", "Return" all work).
---@param key string|integer key name (e.g. "Return", "Tab", "A") or Qt key code
---@param ... string optional modifier strings
function QML_Tester:key_click(key, ...) end

---Press a key (without release).
---@param key string|integer
---@param ... string
function QML_Tester:key_press(key, ...) end

---Release a held key.
---@param key string|integer
---@param ... string
function QML_Tester:key_release(key, ...) end

---Type a string of text, one character at a time (with 5ms gaps).
---@param text string
function QML_Tester:type(text) end

---Save a screenshot of the current window to `path` (PNG).
---@param path string
---@return boolean true on success
function QML_Tester:screenshot(path) end

---Start recording user input events on the current window.
function QML_Tester:record_start() end

---Stop recording and return the captured events as a table (array of event maps).
---Events have at least a `type` field: "note" for radapter.note markers (with `data`
---and `t`), or input events ("mousePress", "mouseRelease", "mouseMove", "keyPress",
---"keyRelease", "wheel") with coordinates/button/key fields and a `t` timestamp.
---@return table
function QML_Tester:record_stop() end

---Replay a recorded event log from a JSON file.
---@param path string
---@param speed? number replay speed multiplier (default 1.0)
function QML_Tester:replay(path, speed) end

---Replay events from an inline JSON string.
---@param json string
---@param speed? number
function QML_Tester:replay_data(json, speed) end

---@return QML_Tester
function QML_Tester() end

---@class RedisCacheWorker: Worker
RedisCacheWorker = {}

---@alias RedisCallback fun(result: any, error: string)

---@param query string
---@return fun(defer: RedisCallback)
function RedisCacheWorker:Exec(query) end

---@param query string
---@param callback RedisCallback
---@return nil
function RedisCacheWorker:Exec(query, callback) end

---@param query string
---@param args any[]
---@return fun(defer: RedisCallback)
function RedisCacheWorker:Exec(query, args) end

---@param query string
---@param args any[]
---@param callback RedisCallback
---@return nil
function RedisCacheWorker:Exec(query, args, callback) end

---@class RedisConfig
---@field host string?
---@field port number?
---@field db number?
---@field reconnect_timeout number?

---@class RedisCacheConfig : RedisConfig
---@field hash_key string?
---@field enable_keyevents boolean?

---@return RedisCacheWorker
---@param params RedisCacheConfig
function RedisCache(params) end

---@class RedisStreamConfig : RedisConfig
---@field stream_key string
---TODO: other fields and enums

---@return Worker
---@param params RedisStreamConfig
function RedisStream(params) end

---@return Worker
function WebsocketServer(params) end

---@return Worker
function WebsocketClient(params) end

---@class LocalServerConfig : WorkerConfig
---@field socket string -- local socket / named-pipe name to listen on
---@field protocol ("json"|"msgpack")? -- frame payload encoding (default "json")
---@field compression "zlib"? -- optional payload compression
---@field per_client boolean? -- route per connection ({id=msg}); else broadcast (default false)

---@class LocalClientConfig : WorkerConfig
---@field socket string -- server socket name to connect to
---@field protocol ("json"|"msgpack")? -- frame payload encoding (default "json")
---@field compression "zlib"? -- optional payload compression
---@field reconnect_timeout integer? -- ms between reconnect attempts (default 300)

---Per-client routed local-IPC server (QLocalServer). Inbound arrives as
---{ ["<clientId>"] = payload }; send { ["<clientId>"] = payload } to reply to one.
---Events: { connected = id } / { disconnected = id }.
---@param params LocalServerConfig
---@return Worker
function LocalServer(params) end

---Local-IPC client (QLocalSocket) that reconnects on drop. Inbound messages are
---written to the socket; received frames are emitted; state on the event channel.
---@param params LocalClientConfig
---@return Worker
function LocalClient(params) end

---Fields common to every worker config.
---@class WorkerConfig
---@field name string? -- explicit worker name (else one is generated)
---@field category string? -- log category override

---@class ProcessConfig : WorkerConfig
---@field program string -- executable to run
---@field arguments string[]? -- argv (no shell; passed literally)
---@field working_dir string? -- cwd for the child
---@field autostart boolean? -- start on construction (default true)
---@field merge_stderr boolean? -- fold stderr into the stdout data channel
---@field binary BinaryParams? -- if set, use BinaryWorker msgpack framing for stdout and stdin

---A child process. Without `binary`: stdout arrives as `{stdout = <bytes>}` on the data
---channel; `pipe(proc, fn)` receives each chunk. stderr and lifecycle land on
---`proc.events`: `{stderr}`, `{started}`, `{finished=true, exit_code}` on normal exit /
---`{finished=true, signal=true}` when killed, `{error}`. Inbound strings/bytes are
---written to stdin.
---
---With `binary`: extends BinaryWorker — stdout and stdin use the configured framing
---and msgpack protocol. Inbound messages are arbitrary objects sent as msgpack frames.
---Destroying the worker terminates the child.
---@class ProcessWorker : Worker
ProcessWorker = {}

---Start the process if it is not already running.
function ProcessWorker:Start() end
---Write bytes to the child's stdin; returns the number of bytes written.
---@param data string
---@return integer
function ProcessWorker:Write(data) end
---Request graceful termination (SIGTERM).
function ProcessWorker:Terminate() end
---Force-kill the child (SIGKILL).
function ProcessWorker:Kill() end
---Send a signal to the child process. Accepts a signal name ("INT", "SIGINT",
---"TERM", "HUP", "USR1", "USR2", "QUIT") or a signal number (2, 15, 1, ...).
---SIGTERM and SIGKILL delegate to Terminate/Kill; other signals are Unix-only.
---Raises if the process is not running or the signal name is unknown.
---@param signal string|integer
function ProcessWorker:Signal(signal) end
---Close the child's stdin (EOF).
function ProcessWorker:CloseStdin() end
---@return integer -- the child's PID (0 if not running)
function ProcessWorker:Pid() end
---@return "not_running"|"starting"|"running"
function ProcessWorker:State() end

---@param params ProcessConfig
---@return ProcessWorker
function Process(params) end

---@class HttpConfig : WorkerBaseParams
---@field base_url string? prepended to relative request URLs
---@field user_agent string? User-Agent header (default "radapter")
---@field follow_redirects boolean? follow 3xx redirects (default true)
---@field timeout_ms integer? abort a request after this many ms (default 30000; 0 = no timeout)
---@field response_format ("raw"|"json"|"text")? how to decode response bodies (default "text")
---@field cert_file string? PEM client certificate for mutual TLS
---@field key_file string? PEM private key for mutual TLS

---Per-request options; all fields override the worker config for that request.
---@class HttpRequestOpts
---@field headers table<string, string>? extra request headers
---@field query table<string, string>? query parameters appended to the URL
---@field format ("raw"|"json"|"text")? response body decoding for this request
---@field timeout_ms integer? request timeout override

---@class HttpResponse
---@field status integer HTTP status code
---@field headers table<string, string> response headers
---@field body any decoded body (string|table|userdata per response_format)

---@alias HttpCallback fun(response: HttpResponse, error: string)

---@class HttpWorker : Worker
HttpWorker = {}

---@param url string
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Get(url, opts) end
---@param url string
---@param callback HttpCallback
---@return nil
function HttpWorker:Get(url, callback) end
---@param url string
---@param opts HttpRequestOpts?
---@param callback HttpCallback
---@return nil
function HttpWorker:Get(url, opts, callback) end

---@param url string
---@param body any?
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Post(url, body, opts) end
---@param url string
---@param body any?
---@param opts HttpRequestOpts?
---@param callback HttpCallback
---@return nil
function HttpWorker:Post(url, body, opts, callback) end

---@param url string
---@param body any?
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Put(url, body, opts) end

---@param url string
---@param body any?
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Patch(url, body, opts) end

---@param url string
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Delete(url, opts) end

---@param url string
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Head(url, opts) end

---@param url string
---@param opts HttpRequestOpts?
---@return fun(defer: HttpCallback)
function HttpWorker:Options(url, opts) end

---@param params HttpConfig
---@return HttpWorker
function Http(params) end

---@class AppInfo
---@field executable string -- absolute path to the running radapter binary
---@field dir string -- directory containing the binary
---@field name string -- application name
---@field pid integer -- this process's PID
---@field qt_version string -- runtime Qt version (e.g. "5.15.3")

---Info about the running process, via Qt's QCoreApplication.
---@return AppInfo
function app_info() end

---@class BinaryParams
---@field framing "slip"
---@field protocol "msgpack"
---@field crc "modbus"?

---@class SerialParams : BinaryParams
---@field port string
---@field baud number


---@class SerialWorker : Worker

---@return SerialWorker
---@param params SerialParams
function Serial(params) end

---Reads/writes msgpack-framed messages over stdin/stdout (not stderr).
---The worker name is always "radapter.stdio".
---@class StdioWorker : Worker

---@return StdioWorker
---@param params BinaryParams
function STDIO(params) end



---@class CanFilter
---@field match ("both" | "normal" | "extended")?
---@field type ("remote" | "data" | "error")?
---@field id number
---@field mask number

---@class CanParams
---@field plugin "socketcan" | "virtualcan" | "vectorcan" | "tinycan" | "peakcan" | "systeccan" | "passthrucan"
---@field device string
---@field filters CanFilter[]?
---@field bitrate number?
---@field can_fd boolean?
---@field data_bitrate number?


---@class CanFrame
---@field frame_id string | number
---@field payload string | number
---@field extended_id boolean?
---@field can_fd boolean?

---@class CanWorker : Worker
---@overload fun(frame: CanFrame, source: Worker)

---@return CanWorker
---@param params CanParams
function CAN(params) end

---@alias CyphalType
---| "uavcan.node.ExecuteCommand.Request.1.0"
---| "uavcan.node.ExecuteCommand.Request.1.1"
---| "uavcan.node.ExecuteCommand.Request.1.2"
---| "uavcan.node.ExecuteCommand.Response.1.0"
---| "uavcan.node.ExecuteCommand.Response.1.1"
---| "uavcan.node.ExecuteCommand.Response.1.2"
---| "uavcan.node.GetInfo.Request.1.0"
---| "uavcan.node.GetInfo.Response.1.0"
---| "uavcan.node.GetTransportStatistics.Request.0.1"
---| "uavcan.node.GetTransportStatistics.Response.0.1"
---| "uavcan.node.Health.1.0"
---| "uavcan.node.Heartbeat.1.0"
---| "uavcan.node.ID.1.0"
---| "uavcan.node.IOStatistics.0.1"
---| "uavcan.node.Mode.1.0"
---| "uavcan.node.Version.1.0"
---| "uavcan.node.port.ID.1.0"
---| "uavcan.node.port.List.1.0"
---| "uavcan.node.port.ServiceID.1.0"
---| "uavcan.node.port.ServiceIDList.1.0"
---| "uavcan.node.port.SubjectID.1.0"
---| "uavcan.node.port.SubjectIDList.1.0"
---| "uavcan.primitive.Empty.1.0"
---| "uavcan.primitive.String.1.0"
---| "uavcan.primitive.Unstructured.1.0"
---| "uavcan.primitive.array.Bit.1.0"
---| "uavcan.primitive.array.Integer16.1.0"
---| "uavcan.primitive.array.Integer32.1.0"
---| "uavcan.primitive.array.Integer64.1.0"
---| "uavcan.primitive.array.Integer8.1.0"
---| "uavcan.primitive.array.Natural16.1.0"
---| "uavcan.primitive.array.Natural32.1.0"
---| "uavcan.primitive.array.Natural64.1.0"
---| "uavcan.primitive.array.Natural8.1.0"
---| "uavcan.primitive.array.Real16.1.0"
---| "uavcan.primitive.array.Real32.1.0"
---| "uavcan.primitive.array.Real64.1.0"
---| "uavcan.primitive.scalar.Bit.1.0"
---| "uavcan.primitive.scalar.Integer16.1.0"
---| "uavcan.primitive.scalar.Integer32.1.0"
---| "uavcan.primitive.scalar.Integer64.1.0"
---| "uavcan.primitive.scalar.Integer8.1.0"
---| "uavcan.primitive.scalar.Natural16.1.0"
---| "uavcan.primitive.scalar.Natural32.1.0"
---| "uavcan.primitive.scalar.Natural64.1.0"
---| "uavcan.primitive.scalar.Natural8.1.0"
---| "uavcan.primitive.scalar.Real16.1.0"
---| "uavcan.primitive.scalar.Real32.1.0"
---| "uavcan.primitive.scalar.Real64.1.0"
---| "uavcan.register.Access.Request.1.0"
---| "uavcan.register.Access.Response.1.0"
---| "uavcan.register.List.Request.1.0"
---| "uavcan.register.List.Response.1.0"
---| "uavcan.register.Name.1.0"
---| "uavcan.register.Value.1.0"
---| "uavcan.si.unit.acceleration.Scalar.1.0"
---| "uavcan.si.unit.acceleration.Vector3.1.0"
---| "uavcan.si.unit.angle.Quaternion.1.0"
---| "uavcan.si.unit.angle.Scalar.1.0"
---| "uavcan.si.unit.angular_acceleration.Scalar.1.0"
---| "uavcan.si.unit.angular_acceleration.Vector3.1.0"
---| "uavcan.si.unit.angular_velocity.Scalar.1.0"
---| "uavcan.si.unit.angular_velocity.Vector3.1.0"
---| "uavcan.si.unit.duration.Scalar.1.0"
---| "uavcan.si.unit.duration.WideScalar.1.0"
---| "uavcan.si.unit.electric_charge.Scalar.1.0"
---| "uavcan.si.unit.electric_current.Scalar.1.0"
---| "uavcan.si.unit.energy.Scalar.1.0"
---| "uavcan.si.unit.force.Scalar.1.0"
---| "uavcan.si.unit.force.Vector3.1.0"
---| "uavcan.si.unit.frequency.Scalar.1.0"
---| "uavcan.si.unit.length.Scalar.1.0"
---| "uavcan.si.unit.length.Vector3.1.0"
---| "uavcan.si.unit.length.WideScalar.1.0"
---| "uavcan.si.unit.length.WideVector3.1.0"
---| "uavcan.si.unit.luminance.Scalar.1.0"
---| "uavcan.si.unit.magnetic_field_strength.Scalar.1.0"
---| "uavcan.si.unit.magnetic_field_strength.Scalar.1.1"
---| "uavcan.si.unit.magnetic_field_strength.Vector3.1.0"
---| "uavcan.si.unit.magnetic_field_strength.Vector3.1.1"
---| "uavcan.si.unit.magnetic_flux_density.Scalar.1.0"
---| "uavcan.si.unit.magnetic_flux_density.Vector3.1.0"
---| "uavcan.si.unit.mass.Scalar.1.0"
---| "uavcan.si.unit.power.Scalar.1.0"
---| "uavcan.si.unit.pressure.Scalar.1.0"
---| "uavcan.si.unit.temperature.Scalar.1.0"
---| "uavcan.si.unit.torque.Scalar.1.0"
---| "uavcan.si.unit.torque.Vector3.1.0"
---| "uavcan.si.unit.velocity.Scalar.1.0"
---| "uavcan.si.unit.velocity.Vector3.1.0"
---| "uavcan.si.unit.voltage.Scalar.1.0"
---| "uavcan.si.unit.volume.Scalar.1.0"
---| "uavcan.si.unit.volumetric_flow_rate.Scalar.1.0"
---| "uavcan.time.GetSynchronizationMasterInfo.Request.0.1"
---| "uavcan.time.GetSynchronizationMasterInfo.Response.0.1"
---| "uavcan.time.Synchronization.1.0"
---| "uavcan.time.SynchronizedTimestamp.1.0"
---| "uavcan.time.TAIInfo.0.1"
---| "uavcan.time.TimeSystem.0.1"

---@alias CyphalService
---| "uavcan.node.ExecuteCommand.1.0"
---| "uavcan.node.ExecuteCommand.1.1"
---| "uavcan.node.ExecuteCommand.1.2"
---| "uavcan.node.GetInfo.1.0"
---| "uavcan.node.GetTransportStatistics.0.1"
---| "uavcan.register.Access.1.0"
---| "uavcan.register.List.1.0"
---| "uavcan.time.GetSynchronizationMasterInfo.0.1"


---@class CyphalTopic
---@field type CyphalType
---@field port number

---@class LocalCyphalService
---@field type CyphalService
---@field port number
---@field handler asyncThunk<any, any>

---@class CyphalNodeInfoVersion
---@field major number
---@field minor number

---@class CyphalNodeInfo
---@field protocol_version CyphalNodeInfoVersion?
---@field hardware_version CyphalNodeInfoVersion?
---@field software_version CyphalNodeInfoVersion?
---@field software_vcs_revision_id number?
---@field unique_id string?
---@field name string?
---@field software_image_crc number[]?
---@field certificate_of_authenticity string?


---@class CyphalConfig
---@field can CanWorker
---@field node_id number
---@field heartbeat_period number?
---@field tx_cap number?
---@field subscribe table<string, CyphalTopic>?
---@field publish table<string, CyphalTopic>?
---@field node_info CyphalNodeInfo?
---@field services LocalCyphalService[]?

---@class CyphalRequestParams
---@field type CyphalService
---@field server number
---@field port number
---@field timeout number?


---@class CyphalWorker : Worker
CyphalWorker = {}

---@overload fun(self: CyphalWorker, params: CyphalRequestParams, msg: any, callback: fun(res: any, err: string))
---@overload fun(self: CyphalWorker, params: CyphalRequestParams, msg: any): promise<any>
function CyphalWorker:Request(params, msg, callback) end

---@return CyphalWorker
---@param params CyphalConfig
function Cyphal(params) end

-- Tag system (available only when radapter is run with --tags flag)

---@class TagEvent
---@field name string      tag name in "worker:field" form
---@field value any        current value
---@field quality "good" | "comm_fail"
---@field ts number        milliseconds since epoch

---@class TagInfo
---@field value any
---@field quality "good" | "comm_fail"
---@field ts number

---@class TagChanged: Pipable
---@field [string] Pipable  pipe target scoped to one tag: tags.changed["worker:field"]

---@class TagsApi
---@field changed TagChanged  pipe target firing a TagEvent on every tag update; index by name for one tag
tags = {}

---Subscribe to a specific tag by name. Callback fires immediately on each update.
---@param name string
---@param fn fun(ev: TagEvent)
function tags:subscribe(name, fn) end

---Return the last known value, quality and timestamp for a tag, or nil if unknown.
---@param name string
---@return TagInfo?
function tags:get(name) end

---Return the Worker object that owns the tag, or nil if not yet known.
---@param name string
---@return Worker?
function tags:source(name) end