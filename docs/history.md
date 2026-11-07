# История парковок и расходы (#22)

Владелец — Альберт (`Albe345`). Сервис только читает SQLite и не выполняет `INSERT`/`UPDATE`/`DELETE`. Деньги — целые копейки RUB, время — Unix UTC в секундах.

## HistoryService

```cpp
HistoryService(QSqlDatabase db, SessionContext *context, QObject *parent = nullptr);
Q_PROPERTY(QVariantList entries READ entries NOTIFY historyChanged)
Q_PROPERTY(qint64 totalKopecks READ totalKopecks NOTIFY historyChanged)
Q_PROPERTY(int totalSessions READ totalSessions NOTIFY historyChanged)
Q_INVOKABLE QVariantMap refresh();   // Result из docs/CONTRACTS.md
```

Регистрация в QML: `historyService`. Подключение в сборку: `include(src/history/history.pri)`.

- **`entries`** — до 100 последних сессий текущего пользователя, порядок `startsAtUtc DESC, id ASC`. Поля: `id`, `zoneId`, `zoneName`, `address`, `plate` (`plate_snapshot`), `startsAtUtc`, `expiresAtUtc`, `amountKopecks` (накопленная сумма сессии с продлениями), `currency = "RUB"`, `status`, `ticketNumber = "DEMO-<id>"`, `paymentIsDemo = true`. Источник — `sessions → spaces → zones`.
- Истёкшая `active` (`expires_at_utc <= now`) показывается как `completed`; запись в БД сервис не меняет, завершение делает `BookingService.refresh()`.
- **`totalSessions`, `totalKopecks`** — по **всем** сессиям пользователя, а не только по 100 строкам. Учитывается `amount_kopecks` сессии один раз; `payment_operations` повторно не складываются. Сумма считается в C++ с проверкой предела 2^53 − 1: при превышении — `STORAGE_ERROR`, данные очищаются.
- **`refresh()`**:
  - успех → `data: {totalKopecks, totalSessions, shownSessions}`;
  - без входа → `NOT_AUTHENTICATED`, данные пусты;
  - ошибка чтения или закрытая БД → `STORAGE_ERROR`, данные очищаются (ложного нуля как успешной статистики нет).
- **`SessionContext::userChanged`** → сначала очистка и `historyChanged`, затем загрузка для нового пользователя. Записи прежнего пользователя не видны ни в одном уведомлении.

## HistoryPage (`qml/history/HistoryPage.qml`)

- Сводка «Всего по парковкам» (`PriceLabel`) и число парковок с пометкой «оплаты демонстрационные». Если `totalSessions > entries.length` — подпись «В списке последние N из M».
- Список `SilicaListView`: зона, адрес, машина · дата по местному времени · статус, сумма.
- Отдельные состояния: «Нужен вход», «История не загрузилась» с «Повторить», пустая история (`ViewPlaceholder`).
- `refresh()` вызывается при `PageStatus.Activating` и при возврате в активное приложение; таймера опроса нет. Сигнал `backRequested()`.

## Для интегратора

- В `.pro`: `include(src/history/history.pri)`.
- В `main.cpp`: `HistoryService` с той же `QSqlDatabase` и `SessionContext`, `setContextProperty("historyService", ...)`, затем `refresh()`.
- Соединить `bookingService.activeSessionChanged` → `historyService.refresh()`: новая бронь или продление сразу попадают в историю.
- Добавить переход на `qml/history/HistoryPage.qml` (например, из профиля или меню карты).
- `PriceLabel.amountKopecks real` (#10, ветка `feat/driver-ui`) нужен, чтобы итог больше 2^31 − 1 коп. отображался верно.

## Проверки

- `python3 tests/history/test_history_sql.py` — SQL берётся прямо из `historyservice.cpp` и выполняется на `migrations/001_initial.sql` (12 проверок).
- `tests/history/run_host_tests.sh` — тот же Python и QtTest `tst_historyservice` на настоящих `SessionContext` и `BookingService` (Qt 5 + SQLite на хосте). Хостовый Qt не заменяет сборку в Aurora SDK.
