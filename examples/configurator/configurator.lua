-- Schema-driven GUI configurator for Modbus / Redis / SQL / Websocket workers.
--
-- The radapter config schema (the same tree `--schema` prints) is exposed to Lua
-- via the global schema(). We hand the relevant worker schemas to a QML window
-- (radapter's reusable WorkerConfigurator / SchemaForm components), which renders
-- a form per worker type. When the user clicks "Build", the window emits a
-- declare-compatible config fragment back here; this demo logs it, encodes it,
-- and instantiates it with declare.build.
--
-- Run (needs a Qt GUI):
--   build/bin/radapter --gui examples/configurator/configurator.lua

local declare = require "declare"

-- worker families this configurator offers (devices included so Modbus masters
-- can reference / create one)
local supported = {
    "ModbusMaster", "ModbusSlave",
    "TcpModbusDevice", "RtuModbusDevice", "TcpModbusServer", "RtuModbusServer",
    "RedisCache", "RedisStream",
    "Sql",
    "WebsocketServer", "WebsocketClient",
}

-- only the worker types the user picks directly (devices are reached via the ref
-- field's "New…" flow, so they need a schema but not a top-level entry)
local pickable = {
    "ModbusMaster", "ModbusSlave",
    "RedisCache", "RedisStream",
    "Sql",
    "WebsocketServer", "WebsocketClient",
}

local all = schema()
local schemas = {}
for _, name in ipairs(supported) do
    schemas[name] = assert(all[name], "missing schema for " .. name)
end

-- Bespoke editors for complex fields the auto-generated form handles poorly.
-- Keyed by "<WorkerType>.<field>"; the value is a QML file (loaded by URL). This
-- is the extension point for richer configurators. lfs.currentdir() is the script
-- dir (radapter chdirs there), so we can hand QML absolute file:// URLs.
local dir = lfs.currentdir()
local registers_form = "file://" .. dir .. "/RegistersForm.qml"
local queries_form = "file://" .. dir .. "/QueriesForm.qml"
local custom_forms = {
    ["ModbusMaster.registers"] = registers_form,
    ["ModbusSlave.registers"] = registers_form,
    ["ModbusMaster.queries"] = queries_form,
}

local view = QML {
    url = "./Configurator.qml",
    properties = {
        custom_forms = custom_forms,
    },
}

-- restrict the type picker to the pickable set
view {
    schemas = schemas,
    pickable = pickable,
}

-- receive the built config fragment ({ objects = { ... } }) and act on it
pipe(view, unwrap("config"), function(cfg)
    log.info("Built config: {}", json_encode(cfg))
    local ok, err = pcall(function() declare.build(cfg) end)
    if ok then
        log.info("Instantiated {} object(s)", (function()
            local n = 0
            for _ in pairs(cfg.objects or {}) do n = n + 1 end
            return n
        end)())
    else
        log.error("Could not build config: {}", err)
    end
end)
