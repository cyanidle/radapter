# Redis Adapter

Как радаптер, но на LUA. Система для соединения зоопарка устройств/протоколов/баз данных

## Special thanks

* smokie-l for inspiration
* https://github.com/pkulchenko/MobDebug for remote debugger

## Build
```bash
sudo apt update
sudo apt install \
   cmake ninja-build build-essential \
   libqt5websockets5-dev libqt5serialbus5-dev libqt5serialport5-dev \
   libqt5sql5-mysql libqt5sql5-odbc libqt5sql5-psql libqt5sql5-sqlite

# clone repo
cmake -B build -G Ninja
cmake --build build -j $(nproc)
# we can test it with 
build/bin/radapter tests/basic.lua
```
Рекомендуется объявить переменную окружения CPM_SOURCE_CACHE (для кэширования пакетов)
```bash
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"
echo "export CPM_SOURCE_CACHE=$CPM_SOURCE_CACHE" >> ~/.bashrc # применится после login
```

## --schema
```bash
radapter --schema # -> prints a json with available config description
```

## JIT
Для сборки с поддержкой Just in Time Compiltion:
```bash
sudo apt install libluajit2-5.1-dev #статическая 
cmake -B build -G Ninja -D RADAPTER_JIT=ON
cmake --build build -j $(nproc)
build/bin/radapter tests/basic.lua
```
В Режиме JIT поддерживается [лишь LUA5.1](https://luajit.org/extensions.html) + Extension

## Example
```lua
-- func ({key = value}) -> () are optional for single param
local device = TcpModbusDevice({
   host = "localhost",
   port = 502,
})

-- These are workers (created with Function Calls)
local modbus = ModbusMaster {
   device = device,
   slave_id = 1,
   ... -- config will be validated
}

local ws = WebsocketServer {    
   port = 10701
}

-- pipelines
pipe(modbus, ws)
pipe(ws, modbus)

-- send a msg to websocket (all clients)
ws {
   hello = "world"
}

-- send a msg after 3000ms (like setTimeout)
after(3000, function()
   ws {
      data = 1
   }
end)

-- send a msg each 4000ms (like setInterval)
each(4000, function()
   ws {
      data = 2
   }
end)


```

## Pipeable protocol

* Building data collection pipeline is done using `Workers` and `functions` inside unidirectional `pipes`
* Every connection is done using `pipe()` function.
* `Table` or `Userdata`: Implement `IPipable` interface,
* `Function`: always pipeable. Subscribers will receive return value

```lua
pipe(func1, func2) --> returns 'func1'
pipe(worker1, worker2, func3) --> returns 'worker1'
```

### IPipeable
1) `get_listeners(self) -> table` -> return table of listeners
2) `call(self, msg, sender?)` -> handle incoming
```lua
function MyWorker()
   local listeners = {} -- should be a table!
   return {
      get_listeners = function(self)
         return listeners
      end
      call = function(self, msg)
         -- handle msg
      end,
   }
end
```
* listeners table is just an array of functions, which can be called.
* for convenience `call_all(table, ...)` function is globally available, calls all interies in table with `...` args
```lua
local worker1 = MyWorker()

pipe(worker1, ...)

worker1:get_listeners() -- has some entries

-- we can emit msg by
for i, v in ipairs(worker1:get_listeners()) do
   v({
      data = "msg data"
   })
end
-- OR
call_all(worker1:get_listeners(), {
   data = "msg data"
})
-- OR
notify_all(worker1, {
   data = "msg data"
})
```

### IPipeable for functions
`pipe(<function>)` call will auto-create a IPipeable wrapper

```lua
local function fn(msg)
   log(msg)
end

local result = pipe(fn) 

-- result looks like this:
local like_result = {
   __wrap = fn,
   __listeners = {},
   get_listeners = function(self)
      return self.__listeners
   end,
   call = function(self, msg)
      local temp = self.fn(msg)
      if temp ~= nil then
         call_all(self.listeners, temp, self)
      end
   end
}

```
