-- Declarative radapter setup.
--
-- Instead of constructing workers and wiring pipes imperatively, describe the
-- whole instance as a plain table and hand it to `declare.build`. Because the
-- table is just data (JSON-serializable), it can be saved to / loaded from a
-- local file or a Redis hash for remote deployment -- this is the data model a
-- GUI editor sits on top of.
--
-- The config has two parts:
--   objects = { <name> = { type = <factory>, config = {...} }, ... }
--     Every buildable thing -- workers and shared devices alike. The name (the
--     map key) becomes the worker name. Inside a config, { ref = "<name>" }
--     looks an object up in this same map, so several workers can share one
--     (e.g. two Modbus masters on one device).
--   pipes = { { from = <name>, to = <name>, [wrap|unwrap|on = <key>] }, ... }
--     Connections between objects. The optional transform reuses wrap/unwrap/on.
--
-- Run:
--   build/bin/radapter examples/declarative.lua            -- build inline config
--   build/bin/radapter examples/declarative.lua save       -- save config to Redis
--   build/bin/radapter examples/declarative.lua load       -- build from Redis
-- (the save/load variants need a Redis server on localhost:6379)

local declare = require "declare"

local config = {
    objects = {
        plc = { type = "TcpModbusDevice", config = {
            host = "localhost",
            port = 1502,
        } },
        master = { type = "ModbusMaster", config = {
            device = { ref = "plc" },          -- shared device, looked up by name
            slave_id = 2,
            registers = {
                holding = {
                    ["pump:status"] = { index = 1 },
                    ["pump:speed"] = { index = 3, type = "float32" },
                },
            },
        } },
        cache = { type = "RedisCache", config = {
            hash_key = "pump:state",
        } },
    },
    pipes = {
        -- forward every reading from the master into the Redis cache
        { from = "master", to = "cache" },
        -- and log just the status field as it changes
        { from = "master", to = "cache", on = "pump:status" },
    },
}

local REDIS = { host = "localhost", port = 6379, db = 0, key = "radapter:config" }

local mode = args[1]

if mode == "save" then
    -- Stored as a Redis HASH with one field per object name (plus a reserved
    -- "@pipes" field), so individual objects can be edited remotely.
    declare.save_to {
        config = config,
        host = REDIS.host, port = REDIS.port, db = REDIS.db, key = REDIS.key,
    }
    log.info("Saved declarative config to Redis key '{}'", REDIS.key)
    shutdown()
elseif mode == "load" then
    declare.load_from {
        host = REDIS.host, port = REDIS.port, db = REDIS.db, key = REDIS.key,
    }
    log.info("Built instance from Redis key '{}'", REDIS.key)
else
    declare.build(config)
    log.info("Built instance from inline config")
end
