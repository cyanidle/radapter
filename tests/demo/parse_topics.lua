local os = require "os"
local io = require "io"

---@alias FieldConverter fun(value: any): any

---@class TopicField
---@field name string
---@field converter FieldConverter?

---@param worker Worker
---@param id number
---@param fields TopicField[]
local function make_topic(worker, id, fields)
    local function pack_tuple(msg)
        local tuple = {}
        for i, field in ipairs(fields) do
            local v = msg[field.name]
            if field.converter then tuple[i] = field.converter(v)
            else tuple[i] = v end
        end
        return tuple
    end
    local function unpack_tuple(tup)
        local msg = {}
        for i, field in ipairs(fields) do
            msg[field.name] = tup[i]
        end
        return msg
    end
    local topic = create_worker(function(self, msg)
        worker {id, pack_tuple(msg)}
    end)
    pipe(worker, function (msg)
        local mid = msg[1]
        local mbody = msg[2]
        if mid == id then
            notify_all(topic, unpack_tuple(mbody))
        end
    end)
    return topic
end

local function int_convert(n)
    n = tonumber(n)
    if (n == nil) then return nil
    else return math.floor(n + 0.5) 
    end
end

---@type table<string, FieldConverter>
local rules = {
    String = tostring,
    string = tostring,
    unsigned = int_convert,
    int = int_convert,
    float = tonumber,
    double = tonumber,
}

---@param worker Worker
---@param file string
---@return any
return function (worker, file)
    local f, err = io.open(file, "r")
    if not f then
        error("Could not open: "..file.." -> "..err)
    end
    ---@type string
    local data = f:read("a")
    data = data:gsub("\n", "")
    ---@type table<string, TopicField[]>
    local msgs = {}
    for type, body in data:gmatch("MAKE_MSG%s*%(%s*(%w+)%s*,([^;]*)%)%s*;") do
        msgs[type] = {}
        local current_table = msgs[type]
        for field in string.gmatch(body, "[^,]+") do
            local field_type, field_name  = string.match(field, "%((.*)%)%s*(%w+)")
            local value_converter = nil
            for known_type, converter in pairs(rules) do
                if string.match(field_type, known_type) then
                    value_converter = converter
                end
            end
            current_table[#current_table + 1] = {
                name = field_name,
                converter = value_converter
            }
        end
    end
    local result_topics = {}
    local topics_data = data:match("TOPICS%s*%([^;]*%)%s*;")
    local id = 0
    for name, msg, _ in string.gmatch(topics_data, "%(%s*(%w+)%s*,%s*(%w+)%s*,%s*(%w+)%s*%)") do
        local topic_msg = msgs[msg]
        assert(topic_msg, "Could not find msg type for topic: "..name)
        result_topics[name] = make_topic(worker, id, topic_msg)
        id = id + 1
    end
    --todo: topics
    return result_topics
end