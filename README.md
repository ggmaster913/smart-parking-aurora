# Умная парковка для ОС Аврора

Команда **ИИксперты**, хакатон «Ночь кода», 7–8 ноября 2026 года.

Нативное приложение для ОС Аврора 5: карта демонстрационных парковочных зон, выбор места, расчёт стоимости до оплаты, симуляция оплаты, электронный талон, активная парковка с отсчётом и продлением, история и расходы. Данные хранятся в SQLite на устройстве, внешний сервер не нужен.

- Мос.Хаб: https://hub.mos.ru/event_DIT/2026/iiksperty
- GitHub (рабочий): https://github.com/ggmaster913/smart-parking-aurora
- Презентация и сценарий: [deliverables/](deliverables/), чек-лист сдачи: [docs/submission/SUBMISSION_CHECKLIST.md](docs/submission/SUBMISSION_CHECKLIST.md)

## Что проверено

Общее приложение собрано в Aurora SDK **MB2 5.2.1.200** для таргета `AuroraOS-5.2.1.200-x86_64` (Qt 5.6.3) и пройдено в эмуляторе (PR #29, ветка `integration/app`). Сквозной сценарий:

1. Демо-вход → карта → зона и место.
2. Нет машины → добавление → расчёт **180 ₽** → демо-оплата → талон.
3. Продление на 15 минут, **45 ₽** → перезапуск (вход и сессия сохранились).
4. «Показать зону на карте» → история **225 ₽** → выход и повторный вход.

Снимки и условия: [tests/app/emulator/README.md](tests/app/emulator/README.md). Проверенный RPM: `ru.iiexperts.SmartParking-0.1-5.x86_64.rpm`, SHA-256 `531b574ba2b4d8593fe8fdbddfa288b71003cd9d41ab4f558f00f6875dd815e8` (см. [чек-лист](docs/submission/SUBMISSION_CHECKLIST.md)). Также проверены полностью занятая зона и работа списка без сети и GPS.

Хостовые проверки модулей (Qt 5.15 на macOS, дополнительно к Аврора-сборке): `tests/booking`, `tests/history`, `tests/experience`, `tests/account`, `tests/parking`, `tests/core`. Хостовый запуск не заменяет целевую сборку.

## Сборка

```sh
sfdk -c target=AuroraOS-5.2.1.200-x86_64 build
sfdk deploy --sdk
```

Результат — `ru.iiexperts.SmartParking-<версия>.x86_64.rpm`, подпись тестовым ключом SDK `regular`. Проект на qmake: `ru.iiexperts.SmartParking.pro`. Корень QML — `qml/SmartParking.qml`. Карта MFW ставится в `lib/qmlplugins`, порядок описан в `third_party/mfw/`. При первом запуске система запрашивает разрешения «Геолокация» и «Интернет».

Хостовые тесты модулей, например:

```sh
QMAKE=/path/to/qt5/bin/qmake sh tests/booking/run_host_tests.sh
QMAKE=/path/to/qt5/bin/qmake sh tests/history/run_host_tests.sh
QMAKE=/path/to/qt5/bin/qmake sh tests/experience/run_host_tests.sh
```

## Устройство

QML/Silica отвечает только за интерфейс. Бизнес-правила реализованы в C++-сервисах `AccountService`, `ParkingService`, `BookingService` и `HistoryService` поверх одной SQLite (`DatabaseManager`, `SessionContext`). Деньги хранятся в целых копейках, время — в UTC, SQL параметризован. Бронь и продление идемпотентны по ключу операции. История только читает БД. Подробности: [ARCHITECTURE](docs/ARCHITECTURE.md), [CONTRACTS](docs/CONTRACTS.md), [DESIGN](docs/DESIGN.md), [history](docs/history.md).

## Ограничения прототипа

- Вход демонстрационный: код `123456` показывается на экране, SMS и письма не отправляются.
- Оплата симулирована, реквизиты карты не собираются. Парковочные зоны и занятость вымышлены.
- Реального API, ML, фоновых уведомлений и офлайн-подложки карты нет. Список зон работает без подложки.
- Подбор «Ближе / Дешевле / Баланс» (PR #26): правила и тесты готовы, интерфейс в приложение не подключён.

## Команда и процесс

Роли и фактический вклад, использование ИИ: [BASE_INFO.md](BASE_INFO.md). Процесс работы агентов: [AGENTS.md](AGENTS.md), [TEAM_STATUS](docs/TEAM_STATUS.md).

Собственный код распространяется по MIT: [LICENSE](LICENSE). Лицензии и уведомления сторонних компонентов (MFW, OpenStreetMap) сохраняются. Атрибуция OSM видна на карте.
