
---@param serial Worker
---@param id number
---@param fields string[]
return function(serial, id, fields)
    local function pack_tuple(msg)
        local tuple = {}
        for i, key in ipairs(fields) do
            tuple[i] = msg[key]
        end
        return tuple
    end
    local function unpack_tuple(tup)
        local msg = {}
        for i, key in ipairs(fields) do
            msg[key] = tup[i]
        end
        return msg
    end
    local topic = create_worker(function(self, msg)
        serial {id, pack_tuple(msg)}
    end)
    pipe(serial, function (msg)
        local mid = msg[1]
        local mbody = msg[2]
        if mid == id then
            notify_all(topic, unpack_tuple(mbody))
        end
    end)
    return topic
end