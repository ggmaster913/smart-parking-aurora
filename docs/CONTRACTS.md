# Контракты модулей v0.1

Изменения согласуются в issue с интегратором до реализации. Типы ниже — наши интерфейсы приложения, а не утверждение о наличии таких классов в SDK.

## Общие правила

Идентификаторы — `QString` с UUID или стабильным ID фикстуры. Деньги — целые копейки, валюта RUB. Время — Unix timestamp UTC в секундах (`qint64`), в QML передаётся числом. Даты для пользователя форматируются локально. Общая connection SQLite используется только в главном потоке.

Все команды, кроме явно указанных `void`, возвращают `QVariantMap`:

```json
{"ok": true, "errorCode": "", "message": "", "data": {}}
```

При ошибке `ok=false`, `errorCode` — стабильный код, `message` — русское пояснение, `data={}`. Коды: `INVALID_INPUT`, `NOT_AUTHENTICATED`, `NOT_FOUND`, `NOT_OWNER`, `SPACE_UNAVAILABLE`, `ACTIVE_SESSION_EXISTS`, `SESSION_EXPIRED`, `STORAGE_ERROR`, `IDEMPOTENCY_CONFLICT`. Исключения и SQL-ошибки в QML не пробрасывать. Ошибка не должна оставлять частичную запись.

## Общие классы, владелец — интегратор

`DatabaseManager::open()` возвращает `bool`, `DatabaseManager::connection() const` возвращает открытую `QSqlDatabase`. Не открывать БД молча по другому пути при ошибке. Версию схемы хранить через `PRAGMA user_version`.

`SessionContext` — QObject со свойством `QString userId` только для чтения в QML и сигналом `userChanged()`. Пустая строка означает выход. C++-метод `setUserId(const QString &)` используется AccountService. Сервисы получают `QSqlDatabase` и указатель `SessionContext*` через конструктор, parent — `QObject*`.

## AccountService, владелец — Илья (первоначальный код Матвея)

Свойства с соответствующими notify-сигналами: `bool signedIn`, `QVariantMap profile`, `QVariantList vehicles`.

Методы `Q_INVOKABLE`:

```cpp
QVariantMap demoSignIn(const QString &contact, const QString &code);
QVariantMap saveProfile(const QString &displayName);
QVariantMap addVehicle(const QString &plate, const QString &brand);
void signOut();
void refresh();
```

Сигнал `operationFailed(const QString &errorCode, const QString &message)` сообщает об ошибке выхода. При отказе DELETE `signOut()` сохраняет профиль, автомобили и SessionContext; UI показывает STORAGE_ERROR и не объявляет signedOut. Переход на вход — только после подтверждённого успешного выхода.

Вход явно помечен «Демо». Код `123456` показывается на экране, SMS/письмо не отправляется. Email: trim, lower case, один `@`, непустые части, без пробелов, до 254 символов. Телефон: необязательный `+`, убрать пробелы/скобки/дефисы, затем 10–15 цифр, хранить в нормализованном виде. Это проверка демо-ввода, не подтверждение владения контактом.

`profile`: `id`, `contact`, `displayName`. `vehicles[]`: `id`, `plate`, `brand`. Номер: trim/upper case, длина 4–15, без требования конкретного регионального формата. Марка: trim, 1–60 символов. У одного пользователя одинаковый нормализованный номер не добавляется повторно. Для всех записывающих команд нужен вход. После перезапуска восстанавливаем активного демо-пользователя из локальных настроек БД. При выходе очищаем активный профиль и видимые данные пользователя, не удаляем историю.

## ParkingService, владелец — Илья

Свойства с notify: `QVariantList zones`, `bool hasPosition`, `bool positionIsDemo`.

```cpp
QVariantMap setPosition(double latitude, double longitude, bool isDemo);
QVariantMap zoneDetails(const QString &zoneId) const;
QVariantList spaces(const QString &zoneId) const;
void refresh();
```

`zones[]`: `id`, `name`, `address`, `latitude`, `longitude`, `rateKopecksPerHour`, `workingHours`, `capacity`, `freeSpaces`, `distanceMeters`. Пока координат нет, `distanceMeters=-1`, не сортировать такие значения как ближайшие. Валидные широта/долгота конечны и лежат в [-90,90]/[-180,180]. Расстояние по прямой в метрах, не обещание пешего или автомобильного маршрута. Сортировка: расстояние по возрастанию, затем ID. При отсутствии позиции — по ID.

`spaces[]`: `id`, `zoneId`, `label`, `available`. Место недоступно, если занято в мок-данных либо имеет неистёкшую активную сессию. UI передаёт в бронирование `zoneId`, `spaceId`, а не индекс строки.

## BookingService, сопровождение — Codex (#9), первоначальный код Claude Григория

QML-экраны использует Альберт в #5; C++ и SQL он не меняет.

Свойства с notify: `QVariantMap activeSession`, `QVariantList history`. Пустая map означает отсутствие активной сессии. `history` разрешено оставить пустой до отдельной задачи этапа 4, не выдавая это за реализованную историю.

```cpp
QVariantMap quote(const QString &zoneId, int durationMinutes) const;
QVariantMap book(const QString &zoneId, const QString &spaceId,
                 const QString &vehicleId, int durationMinutes,
                 const QString &operationId);
QVariantMap extend(const QString &sessionId, int additionalMinutes,
                   const QString &operationId);
void refresh();
```

Продолжительность и продление: от 15 до 1440 минут, кратно 15. Стоимость: округление вверх до копейки `ceil(rateKopecksPerHour * minutes / 60)`, вычисляется целочисленно с промежуточным 64-битным произведением. Пример: 18 000 коп./час, 45 минут — 13 500 коп., продление на 15 минут — 4 500 коп. `quote.data`: `amountKopecks`, `durationMinutes`, `currency="RUB"`.

`book` атомарно проверяет текущего пользователя, принадлежность машины, существование места в указанной зоне, доступность и отсутствие другой активной сессии этого пользователя. Затем записывает сессию и успешную симуляцию оплаты в одной транзакции. Ошибка/отмена до подтверждения ничего не бронирует. Поля банковской карты отсутствуют.

Одна активная сессия на пользователя и одно занятое сессией место одновременно. Сессия: `id`, `zoneId`, `spaceId`, `vehicleId`, `plate`, `startsAtUtc`, `expiresAtUtc`, `amountKopecks`, `currency`, `status`, `ticketNumber`, `paymentIsDemo=true`. Номер талона — `DEMO-` + ID сессии, без заявления о юридическом платёжном документе.

`operationId` создаётся UI один раз при подтверждении. Повтор с тем же ключом и теми же нормализованными параметрами возвращает ранее сохранённый результат без второй сессии/оплаты. Для этого вместе с request_json хранится полный response_json; после последующих продлений старый ответ не пересобирается из текущего состояния сессии. Повтор ключа с другим пользователем, видом операции или параметрами возвращает `IDEMPOTENCY_CONFLICT`, не раскрывает чужие данные. Новый ключ продления добавляет время к сохранённому `expiresAtUtc`, а не к текущему времени. Истёкшую/чужую сессию продлить нельзя. Стоимость продления берётся из сохранённого тарифа сессии, чтобы изменение каталога не изменило договорённость.

Таймер отображает `max(0, expiresAtUtc - nowUtc)`. Пересчитывается после пробуждения приложения и перезапуска. Запуск UI-таймера не создаёт новую сессию. `refresh` переводит истёкшие сессии в `completed`, сохраняет талон и освобождает место.

## Схема SQLite, миграции — интегратор

Все ID — TEXT, денежные поля/времена — INTEGER. Не использовать `REAL` для денег. Все запросы с пользовательским вводом параметризованы.

| Таблица | Поля и ограничения | Кто пишет |
|---|---|---|
| `users` | `id PK`, `contact UNIQUE NOT NULL`, `display_name NOT NULL` | AccountService |
| `vehicles` | `id PK`, `user_id FK users`, `plate`, `brand`, UNIQUE(user_id,plate) | AccountService |
| `settings` | `key PK`, `value`; ключ `active_user_id` | AccountService |
| `zones` | `id PK`, `name`, `address`, `latitude`, `longitude`, `rate_kopecks_per_hour >=0`, `working_hours` | Первичный импорт интегратора |
| `spaces` | `id PK`, `zone_id FK zones`, `label`, `mock_occupied` 0/1, UNIQUE(zone_id,label) | Первичный импорт интегратора |
| `sessions` | `id PK`, `user_id FK`, `vehicle_id FK`, `space_id FK`, `starts_at_utc`, `expires_at_utc > starts_at_utc`, `rate_kopecks_per_hour`, `amount_kopecks >=0`, `status` active/completed, `plate_snapshot` | BookingService |
| `payment_operations` | `id PK` = operationId, `user_id FK`, `session_id FK`, `kind` book/extend, `request_json`, `response_json`, `amount_kopecks >=0`, `created_at_utc` | BookingService |

Нужны уникальные частичные индексы активной сессии по `user_id` и `space_id`. До применения проверить поддержку в SQLite выбранного SDK. Истёкшие сессии завершать в транзакции до вставки/продления. Не удалять/пересеивать пользовательские таблицы при каждом запуске.

## Формат мок-данных

`data/parking-zones.json`, UTF-8. Все зоны вымышлены, Москва выбрана только как демонстрационная область. Пример формы, окончательную фикстуру создаёт Илья:

```json
{"schemaVersion":1,"isDemo":true,"zones":[
  {"id":"demo-center","name":"Демо-парковка Центр",
   "address":"Демонстрационный адрес 1","latitude":55.7521,"longitude":37.6177,
   "rateKopecksPerHour":18000,"workingHours":"Круглосуточно",
   "spaces":[{"id":"demo-center-01","label":"01","occupied":false}]}
]}
```

Проверить версию, уникальные ID, обязательные поля, координаты, целый неотрицательный тариф и непустой список мест. Финальная фикстура: минимум пять зон, в том числе полностью занятая и с бесплатным тарифом. Данные импортируются однократно в новую БД; повторный запуск не перетирает активные сессии.

## HistoryService

Реализован отдельным read-only сервисом, QML context `historyService`. Контракт, поля, предел100строк/итоги по всем сессиям и очистка при смене пользователя: [docs/history.md](history.md). `BookingService.history` не используется как готовый источник истории.
