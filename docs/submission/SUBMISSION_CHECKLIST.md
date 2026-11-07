# Чек-лист сдачи

Статусы: ✓ готово · ◐ ждёт финального шага · ○ нет. Обновлять фактами, без предположений.

| Что | Где | Статус |
|---|---|---|
| Исходный код | Мос.Хаб: https://hub.mos.ru/event_DIT/2026/iiksperty · GitHub: https://github.com/ggmaster913/smart-parking-aurora | ◐ публикация на Мос.Хаб — Илья, #31 |
| Релизная ветка и SHA | `main` после слияния PR #29, точный SHA — Codex | ◐ |
| README | [README.md](../../README.md) | ✓ |
| Базовый файл | [BASE_INFO.md](../../BASE_INFO.md) | ✓ |
| Лицензия | [LICENSE](../../LICENSE), MIT; лицензии MFW и атрибуция OSM сохранены | ✓ |
| Презентация PPTX | [deliverables/SmartParking_ИИксперты.pptx](../../deliverables/SmartParking_ИИксперты.pptx) | ◐ |
| Презентация PDF | [deliverables/SmartParking_ИИксперты.pdf](../../deliverables/SmartParking_ИИксперты.pdf) | ◐ |
| Сценарий выступления, 5 мин | [deliverables/DEMO_SCRIPT.md](../../deliverables/DEMO_SCRIPT.md) | ✓ |
| Проверенный RPM | `ru.iiexperts.SmartParking-0.1-4.x86_64.rpm`, SHA-256 `59cdfa162e6a7f859569d764a4244b342f362daf2384e0df34c4338f48aa257b` (PR #29); затем `0.1-5`, SHA-256 `531b574b…15e8`; финальный — в #31 | ◐ финальный SHA из #31 |
| Снимки сквозного сценария | `tests/app/emulator/` (e2e-01…17) | ✓ |
| Резерв демо | снимки по порядку из DEMO_SCRIPT.md | ✓ |

Перед загрузкой на Мос.Хаб проверить: презентация в папке команды, README и BASE_INFO в корне, ссылки открываются без входа.
