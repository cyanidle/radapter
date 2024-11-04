local sql = Sql {
    type = "QSQLITE",
    db = "/home/alexej/repos/radapter/build/test.sqlite",
}

sql:Exec("DROP TABLE users")
sql:Exec("CREATE TABLE users (id text, name text, email text)")

local id = 1

each(1000, function ()
    sql:Exec("INSERT INTO users (id, name, email) VALUES(?, ?, ?)", {
        id, "user_"..id, "user_"..id.."@test.com"
    })
    id = id + 1
end)

each(1000, function ()
    local rows = sql:Exec("SELECT * FROM users")
    log("SELECT: {}", rows)
end)