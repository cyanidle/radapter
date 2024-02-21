local j = parseJson[[
{
    "a": [1, 2, 3],
    "b": "c",
    "d": {
        "lol": "kek"
    }
}
]]
print(assert(#j.a))
print(assert(j.a[1]))
print(assert(j.a[2]))
print(assert(j.a[3]))
print(assert(j.b))
print(assert(j.d.lol))

log.debug(j)
log.debug("{:p}", j)
