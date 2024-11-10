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

---@enum (key) loggingLevel
loggingLevel = {
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
}

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

---@class Pipable
---@field get_listeners fun(self: Pipable): (fun(msg))[]
---@field call fun(self: Pipable, msg: any)

---@class Worker: Pipable
---@overload fun(msg: any)

---@alias pipeInput (Worker | Pipable | fun(msg): any)

---@generic T1 : pipeInput
---@param first T1
---@vararg pipeInput
---@return T1
function pipe(first, ...) end

---@generic T1 : pipeInput
---@param all pipeInput[]
---@return Pipable
function pipe(all) end

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
---@param callback SqlCallback?
---@return nil
function SqlWorker:Exec(statement, callback) end

---@param statement string
---@param params table
---@param callback SqlCallback?
---@return nil
function SqlWorker:Exec(statement, params, callback) end

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


---@class RedisCacheWorker: Worker
RedisCacheWorker = {}

---@param query string
---@param callback fun(result: any, error: string)?
---@return nil
function RedisCacheWorker:Exec(query, callback) end

---@param query string
---@param args any[]
---@param callback fun(result: any, error: string)?
---@return nil
function RedisCacheWorker:Exec(query, args, callback) end

---@return RedisCacheWorker
function RedisCache(params) end
---@return Worker
function RedisStream(params) end


---@return Worker
function WebsocketServer(params) end