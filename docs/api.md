# API лампы

Вся работа с лампой идёт по HTTP (`http://lampa.local`) и WebSocket (`ws://lampa.local:81`).
Ответы — JSON в UTF-8, CORS открыт: веб-интерфейс и Android-приложение используют один и тот же контракт.

## Состояние

### `GET /api/state`

```json
{
  "power": true,
  "brightness": 160,
  "effect": "aurora",
  "color": "#FFB066",
  "speed": 128,
  "intensity": 160,
  "currentLimit": 9000,
  "tz": "Europe/Moscow",
  "manualTime": false,
  "clock": { "colorMode": 0, "speed": 80, "showDate": true, "hour12": false },
  "mapping": { "serpentine": true, "reverse": false, "flipY": false, "offset": 0 },
  "playlist": { "enabled": false, "intervalMin": 15 },
  "alarms": [
    {
      "enabled": true, "days": 31, "hour": 7, "minute": 0,
      "durationMin": 15, "maxBrightness": 200,
      "snoozeMin": 9, "afterOffMin": 0, "lastFiredYday": 0
    }
  ],
  "device": {
    "fw": "1.1.0-pre1", "ip": "192.168.1.42", "rssi": -54,
    "uptime": 3600, "heap": 20704,
    "mac": "A4:CF:12:AA:BB:CC", "time": "2026-11-26T21:30:00+03:00", "ap": false
  },
  "sunrise": { "active": false, "snoozing": false, "progress": 0, "alarmId": -1 },
  "ny": { "days": 36 }
}
```

- `brightness` — 0…255.
- `color` — `#RRGGBB`.
- `speed`, `intensity` — 0…255, влияют на эффекты, где поддерживаются.
- `currentLimit` — ограничение тока блока питания в мА, 500…15000 (по умолчанию 9000).
- `tz` — часовой пояс: имя IANA из списка `GET /api/timezones` (`Europe/Moscow`) либо
  POSIX-строка без слэша (`MSK-3`, `CET-1CEST,M3.5.0,M10.5.0/3`).
- `manualTime` — `true`, если время задано вручную и NTP остановлен.
- `alarms[].days` — битовая маска, бит 0 = понедельник, бит 6 = воскресенье.
- `sunrise.progress` — 0…1; `alarmId` = -1, если рассвет не идёт.
- `ny.days` — сколько дней осталось до Нового года (calc на устройстве).

### `POST /api/state`

Частичное обновление — присылайте только изменившиеся поля.

```bash
curl -X POST http://lampa.local/api/state \
  -H "Content-Type: application/json" \
  -d '{"power":true,"brightness":200,"effect":"fire"}'
```

Патч поддерживает: `power`, `brightness`, `effect`, `color`, `speed`, `intensity`, `currentLimit`, `tz`,
`clock{colorMode,speed,showDate,hour12}`, `mapping{serpentine,reverse,flipY,offset}`,
`playlist{enabled,intervalMin}`. Ответ — новое полное состояние.

## Время и часовые пояса

### `GET /api/timezones`

Массив имён IANA (465 зон), которые понимает прошивка. Отдаётся потоком, чтобы не
занимать RAM:

```json
["Africa/Abidjan", "Africa/Accra", "…", "Europe/Moscow", "UTC"]
```

Таблица IANA → POSIX хранится во flash; клиентам достаточно присылать имя зоны в
`tz`. Можно передавать и готовую POSIX-строку — она применяется как есть.

### Ручная установка времени

```bash
curl -X POST http://lampa.local/api/action \
  -H "Content-Type: application/json" \
  -d '{"action":"settime","epoch":1795641600}'
```

`epoch` — Unix-время UTC (секунды). Часы останавливают NTP и идут от заданного
значения; `state.manualTime` становится `true`.

Вернуть автосинхронизацию:

```json
{ "action": "timens" }
```

## Эффекты

### `GET /api/effects`

```json
[
  { "id": "solid", "name": "Сплошной цвет", "group": "atmo", "color": true, "speed": false, "intensity": false },
  { "id": "fire",  "name": "Камин",         "group": "ny",   "color": false, "speed": true,  "intensity": true }
]
```

`group`: `atmo` — атмосфера, `ny` — новогодние, `time` — часы и отсчёт.
`color/speed/intensity` — использует ли эффект соответствующий параметр.

Порядок: `solid`, `candle`, `breathing`, `aurora`, `ocean`, `starfield`, `rainbow`,
`plasma`, `fire`, `snowfall`, `garland`, `spiral`, `candy`, `fireworks`, `clock`,
`nycountdown`, `nyshow`.

## Будильники

### `PUT /api/alarms`

```json
{
  "alarms": [
    {
      "enabled": true,
      "days": 31,
      "hour": 6,
      "minute": 45,
      "durationMin": 15,
      "maxBrightness": 200,
      "snoozeMin": 9,
      "afterOffMin": 0
    }
  ]
}
```

До 4 будильников. `durationMin` — 5…30, `maxBrightness` — 5…255,
`snoozeMin` — 0…15 (0 = выключено), `afterOffMin` — 0…180 (0 = не выключать).
`lastFiredYday` сохраняется устройством, передавать его не нужно.

## Действия

### `POST /api/action`

```json
{ "action": "sunrise_test" }
```

| Действие | Что делает |
| --- | --- |
| `sunrise_test` | Тестовый рассвет 45 секунд; прежние свет и яркость восстанавливаются |
| `sunrise_stop` | Остановить рассвет/отсрочку |
| `sunrise_snooze` | Отсрочка на `snoozeMin` текущего будильника |
| `settime` | Ручное время: `{"action":"settime","epoch":1795641600}` |
| `timens` | Вернуться к автосинхронизации NTP |
| `calibrate` | Узор калибровки: `{"action":"calibrate","pattern":"grid","ms":20000}` |
| `nyshow` | Запустить новогоднее шоу на 120 секунд |
| `reboot` | Перезагрузка |
| `factory` | Сброс настроек и Wi-Fi |

`pattern`: `grid` (сетка), `chase` (бегущая точка), `off` (остановить узор).

## Wi-Fi и обновление

| Метод | Описание |
| --- | --- |
| `GET /api/scan` | Список сетей: `[{ "ssid": "...", "rssi": -54, "secure": true }]` |
| `POST /api/wifi` | `{"ssid":"Home","pass":"..."}` — сохранить и перезагрузиться |
| `POST /api/ota` | `multipart/form-data`, поле `firmware` — файл `.bin`; лампа перезагрузится |
| `GET /api/update/check` | Проверить GitHub на новую прошивку |
| `POST /api/update/install` | Скачать и установить найденную прошивку |

### Проверка и установка с GitHub

```json
GET /api/update/check
{
  "current": "1.1.0-pre1",
  "latest": "1.1.0-pre2",
  "available": true,
  "url": "https://github.com/…/firmware.bin",
  "size": 512000,
  "notes": "Заметки к релизу"
}
```

Если проверить не удалось (нет сети, GitHub недоступен), вернётся `200` с
`"error"` и `current`. Лампа смотрит релизы `denvisss/lampa`, берёт первый
нечерновой и ищет в нём asset `firmware.bin`.

```json
POST /api/update/install   (тело можно пустое)
{ "ok": true }
```

Установка идёт в фоне: лампа скачивает образ и перезагружается. Прогресс
приходит по WebSocket сообщением `ota` (см. ниже). Если прошивка уже
актуальна — `409 {"error":"already latest"}`, если идёт другая операция —
`409 {"error":"busy"}`, если проверка не удалась — `502`.

## WebSocket

Адрес: `ws://lampa.local:81/` (порт 81, отдельно от HTTP 80).

Сервер присылает:

```json
{ "type": "state", "data": { ...как GET /api/state... } }
{ "type": "sunrise", "active": true, "snoozing": false, "progress": 0.42 }
{ "type": "ota", "phase": "downloading", "progress": 37, "error": "" }
```

- `state` — при подключении, после изменений (с задержкой ~150 мс) и раз в 15 секунд.
- `sunrise` — каждые 700 мс, пока идёт рассвет или отсрочка.
- `ota` — фазы `connecting`, `downloading`, `finalizing`, `done`, `error`;
  `progress` — 0…100, `error` заполнен при сбое.

Клиент может присылать:

```json
{ "type": "preview", "on": true, "fps": 6 }
{ "type": "ping" }
```

После `preview:on` сервер шлёт бинарные кадры **ровно по 768 байт**:
256 пикселей × RGB, логический порядок `i = x + 16*y` (x — кольцо вокруг трубки, y — высота),
без гаммы и ограничения тока, уже с учётом яркости. `fps` — 1…15.

Вместо `ping` можно использовать обычные ping-фреймы WebSocket: сервер отвечает `pong`.

## Коды ошибок

- `400` — некорректный JSON, параметры или неизвестный часовой пояс.
- `404` — неизвестный путь.
- `409` — занято (`busy`) или обновление не требуется (`already latest`).
- `500` — ошибка обновления прошивки.
- `502` — не удалось проверить GitHub или в релизе нет `firmware.bin`.
