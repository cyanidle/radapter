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
   libqt5sql5-mysql libqt5sql5-odbc libqt5sql5-psql libqt5sql5-sqlite \
   qtdeclarative5-dev libqt5quickcontrols2-5

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
Для сборки с поддержкой Just in Time Compilation:
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
