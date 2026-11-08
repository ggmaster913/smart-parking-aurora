# Выход из профиля в эмуляторе (08.11.2026, 01:55–01:58 МСК)

**Среда:** Windows 11 x64, Aurora SDK `5.2.1.200-mb2`, таргет `AuroraOS-5.2.1.200-x86_64` (Qt 5.6.3),
эмулятор «Aurora OS эмулятор 5.2.1.200» (QEMU). Код ветки `feat/3-account-ilya` @ `0ca0656`.

**Стенд (вне репозитория, не каркас приложения):** каркас Матвея из PR18 (`feat/1-bootstrap`),
настоящий core из PR6 (`feat/2-core-storage`: `DatabaseManager`, `SessionContext`, `001_initial.sql`,
БД `parking.sqlite` в каталоге данных приложения) и модуль `src/account` + `qml/account`.
Стартовая страница — `ProfilePage`, если `accountService.signedIn`, иначе `SignInPage`.
Тестовый переключатель стенда: файл `readonly-test` в каталоге данных включает `PRAGMA query_only`
(создаётся и удаляется по SSH), чтобы получить настоящую ошибку `DELETE` при выходе.

Сборка: `sfdk -c target=AuroraOS-5.2.1.200-x86_64 build` → `ru.iiexperts.SmartParking-0.1-1.x86_64.rpm`
без ошибок и предупреждений; подпись тестовым ключом SDK (`regular`); `sfdk deploy --sdk` —
`Installation successful`. Запуск — `runtime-manager-tool Control start ru.iiexperts.SmartParking`.

| Снимок | Шаг и результат |
|---|---|
| `emu-1-profile.jpg` | Демо-вход телефоном `9991234567` и кодом `123456` → профиль |
| `emu-2-pulldown-signout.jpg` | Включён `readonly-test`; вытягиваемое меню «Выйти» |
| `emu-3-signout-error.jpg` | «Не удалось выйти: Не удалось сохранить данные на устройстве…», страница профиля осталась |
| `emu-4-restart-still-signed-in.jpg` | Перезапуск процесса (новый PID в 01:56:43): вход сохранён — интерфейс не обманывал |
| `emu-5-signout-ok.jpg` | Флаг снят, «Выйти» → страница входа |
| `emu-6-restart-signed-out.jpg` | Перезапуск (PID в 01:57:34): страница входа, вход не вернулся |

Не проверено здесь: общий каркас из `integration/app` (сервисы там ещё не подключены) и
добавление автомобиля в эмуляторе (покрыто host-тестами).
