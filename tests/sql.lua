local a = async

local sql = Sql {
    type = "QSQLITE",
    db = "test.sqlite",
}

local ok, err = pcall(function ()
    sql:Exec("DROP TABLE users")
end)

if not ok then
    log.error("Could not delete table: {}", err)
end

sql:Exec("CREATE TABLE users (id text, name text, email text)")

local id = 1

each(1000, function ()
    sql:Exec("INSERT INTO users (id, name, email) VALUES(?, ?, ?)", {
        id, "user_"..id, "user_"..id.."@test.com"
    })
    id = id + 1
end)

each(1000, a.sync(function ()
    sql:Exec("SELECT * FROM users", function(rows, err)
        log("SELECT: {}", rows)
    end)
    local rows, err = a.wait(sql:Exec("SELECT * FROM users LIMIT 1"));
    log("Async select 1: res {}, err {}", rows, err)
    log("Async select 2 (err): res {}, err {}", a.wait(sql:Exec("SLECT * FROM users LIMIT 1")))
end))