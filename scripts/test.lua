local cli = redis.Client.new{}

function StartCount()
    each(300, function ()
        cli:Execute("INCR connects", function(res, err)
            if err then 
                log.error("Could not INCR: {}", err)
            else
                log.info("Current connects: {}", res)
                if res >= 30 then
                    ResetCount()
                end
            end
        end)
    end)
end

function ResetCount()
    cli:Execute "SET connects 0"
end

cli:OnConnected(function()
    log.debug("Client connected")
    StartCount()
end)

cli:OnError(function(err, code)
    log.debug("Error: {}, Code: {}", err, code)
end)

cli:OnDisconnected(function(code)
    log.debug("Disconnected with code: {}", code)
end)

cli:Connect()
