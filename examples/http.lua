-- HTTP client demo.
--
-- The `Http` worker exposes Qt's QNetworkAccessManager as simple Lua methods:
--   Get / Post / Put / Patch / Delete / Head / Options
--
-- Each method returns an awaitable promise; pass a trailing callback instead to
-- get the result via (response, error). A response is a table:
--   { status = <int>, headers = <table>, body = <decoded per response_format> }
--
-- Run with:
--   build/bin/radapter examples/http.lua

local api = Http {
    base_url = "https://httpbin.org",
    response_format = "json", -- decode bodies as JSON (also "text" or "raw")
    timeout_ms = 10000,
}

-- await form (top-level await works; the script runs inside a coroutine)
local r = await(api:Get("/get", { query = { hello = "world" } }))
log.info("GET /get -> status {}, args = {}", r.status, r.body.args)

-- A table body is JSON-encoded automatically and Content-Type is set.
local p = await(api:Post("/post", { name = "radapter", count = 42 }))
log.info("POST /post echoed json = {}", p.body.json)

-- Custom headers per request.
local h = await(api:Get("/headers", { headers = { ["X-Demo"] = "1" } }))
log.info("server saw headers = {}", h.body.headers)

-- Callback form: pass a function as the last argument.
api:Get("/uuid", function(res, err)
    if err then
        log.error("request failed: {}", err)
    else
        log.info("GET /uuid -> {}", res.body.uuid)
    end
    shutdown()
end)
