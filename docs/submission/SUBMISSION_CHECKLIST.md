# Чек-лист сдачи

Статусы: ✓ готово · ◐ ждёт финального шага · ○ нет. Обновлять фактами, без предположений.

| Что | Где | Статус |
|---|---|---|
| Исходный код | Мос.Хаб: https://hub.mos.ru/event_DIT/2026/iiksperty · GitHub: https://github.com/ggmaster913/smart-parking-aurora | ◐ Мос.Хаб: проект закрыт, нужен вход владельца и его согласие; публикацию отложил участник (#31). Публикует Илья одобренный SHA из main |
| Релизная ветка и SHA | `main` после слияния PR #29, точный SHA — Codex | ◐ |
| README | [README.md](../../README.md) | ✓ |
| Базовый файл | [BASE_INFO.md](../../BASE_INFO.md) | ✓ |
| Лицензия | [LICENSE](../../LICENSE), MIT; лицензии MFW и атрибуция OSM сохранены | ✓ |
| Презентация PPTX | [deliverables/SmartParking_ИИксперты.pptx](../../deliverables/SmartParking_ИИксперты.pptx), 6 слайдов по шаблону финалиста | ✓ |
| Презентация PDF | [deliverables/SmartParking_ИИксперты.pdf](../../deliverables/SmartParking_ИИксперты.pdf) | ◐ |
| Сценарий выступления, 5 мин | [deliverables/DEMO_SCRIPT.md](../../deliverables/DEMO_SCRIPT.md) | ✓ |
| Проверенный RPM | `ru.iiexperts.SmartParking-0.1-5.x86_64.rpm`, SHA-256 `531b574ba2b4d8593fe8fdbddfa288b71003cd9d41ab4f558f00f6875dd815e8`; исходник — дерево `60af75d` + `Release: 5`, ветка `feat/app-integration-ilya` @ `bb70db5` (PR #29) | ✓ |
| Снимки сквозного сценария | `tests/app/emulator/` (e2e-01…18; e2e-06/07/09 — до исправления PriceLabel, не показывать) | ✓ |
| Резерв демо | снимки по порядку из DEMO_SCRIPT.md | ✓ |

Перед загрузкой на Мос.Хаб проверить: презентация в папке команды, README и BASE_INFO в корне, ссылки открываются без входа.
