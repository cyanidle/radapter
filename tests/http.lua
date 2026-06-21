-- HTTP worker test: spawns a tiny local HTTP server (python3) and exercises the
-- Http worker's verb methods, body encoding, query/header opts, the await and
-- callback forms, and error propagation. Self-checking: exits 0 with
-- "HTTP test OK", exits 1 on timeout/failure.
--
-- Run with:
--   build/bin/radapter tests/http.lua

local os = require "os"

local PORT = 18099

local server_py = [[
import http.server, json, sys
class H(http.server.BaseHTTPRequestHandler):
    def _send(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(b)))
        self.end_headers()
        self.wfile.write(b)
    def do_GET(self):
        self._send({'method': 'GET', 'path': self.path})
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(n).decode()
        self._send({'method': 'POST', 'echo': body,
                    'ct': self.headers.get('Content-Type'),
                    'x_test': self.headers.get('X-Test')})
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', ]] .. PORT .. [[), H).serve_forever()
]]

local server = Process { program = "python3", arguments = { "-c", server_py } }

local checks = {
    get_await = true,
    post_json = true,
    query_and_headers = true,
    get_callback = true,
    connection_error = true,
}

local function pass(name)
    assert(checks[name], "unexpected/duplicate pass: " .. name)
    checks[name] = nil
    log.info("PASS: {}", name)
    if next(checks) == nil then
        log.info("HTTP test OK")
        server:Terminate()
        shutdown()
    end
end

after(10000, function()
    local missing = {}
    for k in pairs(checks) do missing[#missing + 1] = k end
    log.error("HTTP test FAILED, missing: {}", missing)
    os.exit(1)
end)

local http = Http { base_url = "http://127.0.0.1:" .. PORT, response_format = "json" }

-- Give the server a moment to bind, then drive the requests from a coroutine.
after(800, async(function()
    local r = await(http:Get("/hello"))
    assert(r.status == 200, "GET status: " .. tostring(r.status))
    assert(r.body.method == "GET", "GET method")
    assert(r.body.path == "/hello", "GET path: " .. tostring(r.body.path))
    pass("get_await")

    local p = await(http:Post("/x", { a = 1, b = "two" }))
    assert(p.body.method == "POST", "POST method")
    assert(p.body.ct == "application/json", "auto json content-type: " .. tostring(p.body.ct))
    assert(p.body.echo:find('"a"'), "POST echoed json body: " .. tostring(p.body.echo))
    pass("post_json")

    local q = await(http:Get("/q", { query = { foo = "bar" }, headers = { ["X-Test"] = "1" } }))
    assert(q.body.path == "/q?foo=bar", "query string: " .. tostring(q.body.path))
    pass("query_and_headers")

    -- Callback form (no await).
    http:Get("/cb", function(res, err)
        assert(not err, "callback error: " .. tostring(err))
        assert(res.body.path == "/cb", "callback path: " .. tostring(res.body.path))
        pass("get_callback")
    end)

    -- Error propagation: connection refused on a dead port -> (nil, err).
    local bad = Http { timeout_ms = 1000 }
    bad:Get("http://127.0.0.1:1/nope", function(res, err)
        assert(res == nil, "error case should have no response")
        assert(err and #err > 0, "expected error string")
        pass("connection_error")
    end)
end))
