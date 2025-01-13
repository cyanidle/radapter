assert(args[1], "Please specify plugins dir")

local test = load_plugin(args[1].."/radapter_test_plugin")

local worker = test {
    delay = 2000
}

pipe(worker, function (msg)
    log.info(msg)
end)

worker("Radapter!")