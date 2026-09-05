local mode, name, exe_dir, resolved = unpack(args)

if mode == "load" then
    load_plugin(name, 42, { greeting = "hello" })
    assert(test_plugin_args[1] == 42)
    assert(test_plugin_args[2].greeting == "hello")
    for _, alias in ipairs { name, resolved } do
        local ok, err = pcall(load_plugin, alias)
        assert(not ok and err:find("already loaded", 1, true), tostring(err))
    end
    after(1000, function() error("Plugin worker did not emit") end)
    pipe(TestPlugin { delay = 1 }, function(msg)
        assert(msg.from_plugin == "Hello!")
        print("plugin-search check OK")
        shutdown()
    end)
else
    local ok, err = pcall(load_plugin, name)
    assert(not ok, "Unexpectedly loaded " .. name)
    assert(err:find("Could not load " .. name .. " => ", 1, true), err)
    if mode == "missing" then
        for _, dir in ipairs { "/usr/lib/radapter/plugins", exe_dir, exe_dir .. "/plugins" } do
            assert(err:find(dir .. "/" .. name .. " => ", 1, true), err)
        end
    elseif mode == "explicit_missing" then
        assert(not err:find("/usr/lib/radapter/plugins/", 1, true), err)
        assert(not err:find(exe_dir .. "/plugins/", 1, true), err)
    else
        error("Unknown mode: " .. mode)
    end
    print("plugin-search check OK")
    shutdown()
end
