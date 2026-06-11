assert(args[1], "Please specify plugins dir")

load_plugin(args[1].."/radapter_test_plugin")

local worker = TestPlugin {
    delay = 2000
}

pipe(worker, function (msg)
    log.info(msg)
end)

pipe(worker.events, function (msg)
    log.info("Event => {}", msg)
end)

worker("Radapter!")
