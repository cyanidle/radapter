-- A queued spawn entry that is still pending when the instance is torn down must
-- not settle its promise into a freed lua_State: rpcxx resolves a promise when it
-- is destroyed, and Qt would otherwise drop the entry only after lua_close.
spawn(function ()
    -- if this body ever runs, the entry was dispatched before the teardown, so the
    -- test would cover nothing - fail loudly instead of passing vacuously
    log.error("pending spawn entry was dispatched before the teardown")
    os.exit(1)
end)
shutdown(300)
