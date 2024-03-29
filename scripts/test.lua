local cli = redis.Client.new {
    host="localhost"
}

cli:OnConnected(function()
    log.debug("Client connected")
end)

cli:OnError(function(err, code)
    log.debug("Error: {}, Code: {}", err, code)
end)

cli:OnDisconnected(function(code)
    log.debug("Disconnected with code: {}", code)
end)

cli:Connect()
