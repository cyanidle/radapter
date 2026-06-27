-- Declarative radapter setup: build a graph of workers/devices and the pipes
-- between them from a plain (JSON-serializable) table, and save/load that table
-- to a local file or a Redis hash. See definitions.lua / examples/declarative.lua.

local M = {}

-- Resolve a config value: { ref = "name" } -> the named object; an inline
-- { type = <factory>, config = {...} } -> a freshly built (anonymous) object;
-- any other table is walked recursively; scalars pass through unchanged.
local function resolver(instantiate)
    local function resolve(v)
        if type(v) ~= "table" then
            return v
        end
        if type(v.ref) == "string" then
            return instantiate(v.ref)
        end
        if type(v.type) == "string" and type(v.config) == "table"
            and type(_G[v.type]) == "function" then
            local factory = _G[v.type]
            return factory(resolve(v.config))
        end
        local out = {}
        for k, val in pairs(v) do
            out[k] = resolve(val)
        end
        return out
    end
    return resolve
end

---@param config RadConfig
---@return table<string, Worker>
function M.build(config)
    assert(type(config) == "table", "config table expected")
    local objects = config.objects or {}
    local built = {}
    local building = {}
    local resolve

    local function instantiate(name)
        local existing = built[name]
        if existing ~= nil then
            return existing
        end
        if building[name] then
            error("dependency cycle while building object '" .. name .. "'")
        end
        local entry = objects[name]
        if type(entry) ~= "table" then
            error("unknown object reference: '" .. name .. "'")
        end
        local factory = _G[entry.type]
        if type(factory) ~= "function" then
            error("object '" .. name .. "' has unknown type '" .. tostring(entry.type) .. "'")
        end
        building[name] = true
        local cfg = resolve(entry.config or {})
        cfg.name = cfg.name or name
        local obj = factory(cfg)
        building[name] = nil
        built[name] = obj
        return obj
    end

    resolve = resolver(instantiate)

    for name in pairs(objects) do
        instantiate(name)
    end

    for i, p in ipairs(config.pipes or {}) do
        local from = built[p.from]
        local to = built[p.to]
        assert(from, "pipe #" .. i .. ": unknown source '" .. tostring(p.from) .. "'")
        assert(to, "pipe #" .. i .. ": unknown target '" .. tostring(p.to) .. "'")
        if p.wrap then
            pipe(from, wrap(p.wrap), to)
        elseif p.unwrap then
            pipe(from, unwrap(p.unwrap), to)
        elseif p.on then
            on(from, p.on, to)
        else
            pipe(from, to)
        end
    end

    return built
end

-- Reserved Redis hash fields ('@' cannot appear in an object name, so they never
-- collide with a per-object field): the pipe list and the operator visualization.
local PIPES_FIELD = "@pipes"
local VIZ_FIELD = "@visualization"

local function read_file(path)
    local f, err = io.open(path, "r")
    if not f then
        error("could not open '" .. path .. "': " .. tostring(err))
    end
    local data = f:read("*a")
    f:close()
    return data
end

local function write_file(path, data)
    local f, err = io.open(path, "w")
    if not f then
        error("could not open '" .. path .. "' for writing: " .. tostring(err))
    end
    f:write(data)
    f:close()
end

local function redis_conn(params)
    return RedisCache {
        name = "config_io",
        host = params.host,
        port = params.port,
        db = params.db,
    }
end

---@param params RadSaveParams
function M.save_to(params)
    assert(type(params) == "table", "params table expected")
    local config = params.config
    assert(type(config) == "table", "save_to: 'config' table required")

    if params.path then
        write_file(params.path, json_encode(config))
    end

    if params.key then
        local cache = redis_conn(params)
        local ok, err = pcall(function()
            local fields = {}
            for name, entry in pairs(config.objects or {}) do
                fields[#fields + 1] = name
                fields[#fields + 1] = json_encode(entry)
            end
            fields[#fields + 1] = PIPES_FIELD
            fields[#fields + 1] = json_encode(config.pipes or {})
            if config.visualization then
                fields[#fields + 1] = VIZ_FIELD
                fields[#fields + 1] = json_encode(config.visualization)
            end
            await(cache:Exec("DEL " .. params.key))
            await(cache:Exec("HSET " .. params.key, fields))
        end)
        cache:destroy()
        if not ok then error(err) end
    end

    if not params.path and not params.key then
        error("save_to: provide 'path' (file) and/or 'key' (redis)")
    end
end

-- Read a declarative config table (the { objects, pipes, visualization } shape) from a
-- file or Redis hash, WITHOUT building it — for editors that want the config itself.
---@param params RadLoadParams
---@return RadConfig
function M.read(params)
    assert(type(params) == "table", "params table expected")

    if params.path then
        return json_decode(read_file(params.path))
    elseif params.key then
        local cache = redis_conn(params)
        local ok, res = pcall(function()
            local flat = await(cache:Exec("HGETALL " .. params.key))
            local objects = {}
            local pipes = {}
            local visualization = nil
            for i = 1, #flat, 2 do
                local field, value = flat[i], flat[i + 1]
                if field == PIPES_FIELD then
                    pipes = json_decode(value)
                elseif field == VIZ_FIELD then
                    visualization = json_decode(value)
                else
                    objects[field] = json_decode(value)
                end
            end
            return { objects = objects, pipes = pipes, visualization = visualization }
        end)
        cache:destroy()
        if not ok then error(res) end
        return res
    else
        error("read: provide 'path' (file) or 'key' (redis)")
    end
end

---@param params RadLoadParams
---@return table<string, Worker>
function M.load_from(params)
    return M.build(M.read(params))
end

return M
