-- radapter test suite: runs every self-checking script in tests/ — each in its own
-- radapter process (via the Process worker, so they can't interfere with each other's
-- ports/sockets/shutdown) — and reports a pass/fail summary. Exits 0 iff all pass.
--
--   build/bin/radapter tests/all.lua
--
-- New tests are picked up automatically; add a flag entry below if one needs CLI flags.

local lfs = require "lfs"

local SELF = "all.lua"
local FLAGS = {                       -- per-file extra CLI flags
    ["tags.lua"] = { "--tags" },
}

local TESTS = lfs.currentdir()        -- the tests/ dir (cwd during top-level execution)
local EXE = app_info().executable

local files = {}
for name in lfs.dir(TESTS) do
    if name:match("%.lua$") and name ~= SELF then
        files[#files + 1] = name
    end
end
table.sort(files)

local idx = 0
local failures = {}

local function finish()
    log.info(("==== %d/%d passed ===="):format(#files - #failures, #files))
    for _, f in ipairs(failures) do log.error("  FAILED: {}", f) end
    shutdown()
    os.exit(#failures == 0 and 0 or 1)
end

local function run_next()
    idx = idx + 1
    local file = files[idx]
    if not file then return finish() end

    local argv = {}
    for _, fl in ipairs(FLAGS[file] or {}) do argv[#argv + 1] = fl end
    argv[#argv + 1] = TESTS .. "/" .. file

    local proc = Process { program = EXE, arguments = argv }
    local errbuf = {}
    pipe(proc.events, function(ev)
        if ev.stderr then errbuf[#errbuf + 1] = tostring(ev.stderr) end
        if ev.finished then
            if ev.exit_code == 0 then
                log.info("PASS  {}", file)
            else
                failures[#failures + 1] = file
                log.error("FAIL  {} (exit {})", file, ev.exit_code)
                io.stderr:write(table.concat(errbuf))   -- show the failing run's output
            end
            proc:destroy()
            run_next()
        end
    end)
end

run_next()
