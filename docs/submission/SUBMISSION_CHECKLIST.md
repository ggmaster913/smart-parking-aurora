# Чек-лист сдачи

Статусы: ✓ готово · ◐ ждёт финального шага · ○ нет. Обновлять фактами, без предположений.

| Что | Где | Статус |
|---|---|---|
| Исходный код | Мос.Хаб: https://hub.mos.ru/event_DIT/2026/iiksperty · GitHub: https://github.com/ggmaster913/smart-parking-aurora | ◐ Мос.Хаб: проект закрыт, нужен вход владельца и его согласие; публикацию отложил участник (#31). Публикует Илья одобренный SHA из main |
| Релизная ветка и SHA | `main`, релиз `v0.1.6`; код совпадает с SDK-проверенным `d4eef422` (PR #36) | ✓ |
| README | [README.md](../../README.md) | ✓ |
| Базовый файл | [BASE_INFO.md](../../BASE_INFO.md) | ✓ |
| Лицензия | [LICENSE](../../LICENSE), MIT; лицензии MFW и атрибуция OSM сохранены | ✓ |
| Презентация PPTX | [deliverables/SmartParking_ИИксперты.pptx](../../deliverables/SmartParking_ИИксперты.pptx), 6 слайдов по шаблону финалиста | ✓ |
| Презентация PDF | [deliverables/SmartParking_ИИксперты.pdf](../../deliverables/SmartParking_ИИксперты.pdf), 6 страниц, экспорт PowerPoint (Илья, PR #35) из PPTX SHA-256 `68d1fc5c…61b8`; PDF SHA-256 `bd26445e8325f56d4d907d5ce21c38de129804f1d14c1e7b369d2650d6638ede`; все страницы просмотрены | ✓ |
| Сценарий выступления, 5 мин | [deliverables/DEMO_SCRIPT.md](../../deliverables/DEMO_SCRIPT.md) | ✓ |
| Проверенный RPM | `ru.iiexperts.SmartParking-0.1-6.x86_64.rpm`, SHA-256 `b31093fb9e350217bf6960055586842612cc81e4bf735364a0475fd36e96de0c`; исходник — `273e251` + `Release: 6`, ветка `feat/retry-verify-ilya` @ `d4eef422` (PR #36) | ✓ |
| Снимки сквозного сценария | `tests/app/emulator/` (e2e-01…18 и r6-01…03; e2e-06/07/09 — до исправления PriceLabel, не показывать) | ✓ |
| Резерв демо | снимки по порядку из DEMO_SCRIPT.md | ✓ |

Перед загрузкой на Мос.Хаб проверить: презентация в папке команды, README и BASE_INFO в корне, ссылки открываются без входа.
