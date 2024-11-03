local ws = WebsocketServer{port = 6337}

pipe(ws, log)
pipe{ws, 
    function(msg)
        return msg
    end,
    log
}

local counter = 0
each(1000, function ()
    counter = counter + 1
    ws {data = fmt("This is a msg #{}", counter)}
end)