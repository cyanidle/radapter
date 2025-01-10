---@meta

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

---@param timeout number?
function shutdown(timeout) end

---@param key string
---@return fun(object: any): any
function wrap(key) end

---@param key string
---@return fun(object: any): any
function unwrap(key) end

---@param pattern string
---@return fun(object: any): any
function filter(pattern) end

---@param table any[]
function call_all(table, ...) end

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
---@field levl loggingLevel
---@field msg string
---@field category string
---@field timestamp number

---@param fmt string
---@return string
function fmt(fmt, ...) end

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


---@class Pipable
---@field get_listeners fun(self: Pipable): MsgHandler[]
---@field call MsgHandler


---@class Worker: Pipable
---@overload fun(msg: any, source: Worker)

---@alias pipeInput (Worker | Pipable | MsgHandler)

---@generic T1 : pipeInput
---@param first T1
---@vararg pipeInput
---@return T1
function pipe(first, ...) end

---@param worker Worker
---@param msg any
---@param sender Worker?
function notify_all(worker, msg, sender) end

---@param on_msg MsgHandler
---@return Pipable
function create_worker(on_msg) end

---@alias asyncThunk fun(defer: fun(...))

async = {
    ---@return asyncThunk
    sync = function (func) end,
    ---@return fun(...): asyncThunk
    wrap = function (...) end,
    ---@param thunk asyncThunk
    wait = function (thunk) end,
    ---@param thunks asyncThunk[]
    wait_all = function (thunks) end
}

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


---@class ModbusDevice

---@return Worker
function ModbusMaster (params) end
---@return Worker
function ModbusDevice (params) end

---@return ModbusDevice
function TcpModbusDevice (params) end
---@return ModbusDevice
function RtuModbusDevice (params) end

---@class QMLWorker: Worker
QMLWorker = {}

---@return string
function QMLWorker:dir() end

---@class QMLParams
---@field url string
---@field props string[]

---@return QMLWorker
---@param params QMLParams
function QML(params) end;

---@return QMLWorker
---@param qmlSource string
function QML(qmlSource) end;

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

---@class TempFileObject
TempFileObject = {}

---@return string
function TempFileObject:url() end;

---@return string
function TempFileObject:path() end;

---@return TempFileObject
---@param data string
function temp_file(data) end;

---@class BinaryParams
---@field framing "slip"
---@field protocol "msgpack"

---@class SerialParams : BinaryParams
---@field port string
---@field baud number

---@type string[]
args = {}


---@class SerialWorker : Worker

---@return SerialWorker
---@param params SerialParams
function Serial(params) end
