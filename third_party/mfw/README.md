# MFW (карта) — подключение для «Умной парковки»

Каталог предназначен для **только необходимых** файлов библиотеки MfwMap из официального
шаблона [MapTemplate](https://hub.mos.ru/auroraos/demos/MapTemplate) (© 2024 Open Mobile
Platform LLC, лицензия BSD-3-Clause). Маршрутизация (MfwOfflineRouting, protobuf)
и геокодер (MfwOfflineGeocoding) приложению не нужны и не переносятся.

## Статус на 07.11.2026

- Бинарные файлы **ещё не скопированы**: hub.mos.ru недоступен из облачной среды агента.
  Их копирует Илья на своём компьютере из клона MapTemplate. Ничего не подменять и не
  собирать «похожие» файлы.
- Подключение MFW на SDK BT 5.2.1.200 **не проверено** — сборки и запуска в эмуляторе пока нет.

## Что копировать из MapTemplate

Из `mfw/ru.auroraos/qml/<arch>/MfwMap/` для каждой нужной архитектуры
(`x86_64` — эмулятор, `aarch64`/`armv7hl` — устройства):

```text
third_party/mfw/ru.auroraos/qml/<arch>/MfwMap/qmldir
third_party/mfw/ru.auroraos/qml/<arch>/MfwMap/libMfwMap.so
third_party/mfw/LICENSE.BSD-3-Clause      # текст лицензии из MapTemplate (LICENSE.BSD-3-CLAUSE.md)
```

## Подложка и токены

- Токен Mapbox из примеров MFW **не используется**.
- Подложка: растровые тайлы `https://tile.openstreetmap.org/{z}/{x}/{y}.png` через свойство
  `styleJson` (так подключает растровые тайлы официальный пример MfwMap `Raster.qml`).
  На карте выводится атрибуция «© участники OpenStreetMap», как требует
  [политика OSM](https://operations.osmfoundation.org/policies/tiles/). Для демонстрации на
  хакатоне нагрузка минимальная; для пилота нужен собственный или коммерческий тайл-сервер.
- Без сети подложка не загружается: показывается сообщение, список парковок продолжает
  работать. Офлайн-подложка (MfwOfflineMap) не заявляется, пока не проверена отдельно.

## Нужные изменения общей сборки (выполняет интегратор)

Взято из `app/app.pro`, `mfw/mfw.pro` и `app/src/main.cpp` MapTemplate. Имена путей
подставить по реальному каркасу из #1 (`ru.template.SmartParking` — пример).

1. `.pro` приложения:

   ```qmake
   QT += sql positioning

   GCC_INFO = $$system($$QMAKE_CXX -dumpmachine)
   contains(GCC_INFO, .*aarch64.*) {
       MFW_LIB_FOLDER = aarch64
   } else {
       contains(GCC_INFO, .*armv7hl.*) {
           MFW_LIB_FOLDER = armv7hl
       } else {
           contains(GCC_INFO, .*i486.*) {
               MFW_LIB_FOLDER = i486
           } else {
               MFW_LIB_FOLDER = x86_64
           }
       }
   }
   MFW_DIR = $$PWD/third_party/mfw/ru.auroraos/qml/$${MFW_LIB_FOLDER}/MfwMap
   LIBS += -L$$MFW_DIR -lMfwMap

   MfwMap_install.path = /usr/share/$${TARGET}/lib/qmlplugins/MfwMap
   MfwMap_install.files = $$MFW_DIR/qmldir $$MFW_DIR/libMfwMap.so
   INSTALLS += MfwMap_install

   HEADERS += src/parking/parkingcatalog.h src/parking/parkinggeo.h src/parking/parkingservice.h
   SOURCES += src/parking/parkingcatalog.cpp src/parking/parkinggeo.cpp src/parking/parkingservice.cpp
   ```

   QML-файлы `qml/parking/*` и иконки `qml/parking/icons/*` должны попасть в пакет
   вместе с остальным каталогом `qml/` (как в шаблоне SDK).

2. `src/main.cpp` — до загрузки корневого QML:

   ```cpp
   view->engine()->addImportPath(
       Aurora::Application::pathTo(QStringLiteral("lib/qmlplugins/")).toString());
   ```

   и регистрация сервиса по контракту:
   `view->rootContext()->setContextProperty("parkingService", parkingService);`
   где `parkingService = new ParkingService(db.connection(), sessionContext, app)`.
   После успешной брони/продления вызвать `parkingService->refresh()`.

3. `.desktop`: в `[X-Application] Permissions=` добавить `Location` (геопозиция) и, если ещё
   нет, `Internet` (загрузка тайлов). Точные имена разрешений сверить с документацией SDK.

4. `rpm/*.spec`: `Requires: qt5-qtdeclarative-import-positioning` (QML-модуль QtPositioning,
   по [документации](https://developer.auroraos.ru/doc/software_development/guides/cpp_api/positioning)).
   Исключение библиотек MFW из автоматических зависимостей RPM сверить с `rpm/` MapTemplate
   при первой сборке — не угадывать.

5. Импорт `data/parking-zones.json` в новую БД: `ParkingCatalog::loadFile()` возвращает
   проверенный список зон с местами или пустой список и русскую ошибку — без частичных
   данных. Поле `occupied` → `spaces.mock_occupied` (0/1).

6. Навигация: `qml/parking/ParkingMapPage.qml` — входная страница этапа 2; свойство
   `bookingPageUrl` указывает на страницу подтверждения Альберта, она получает свойства
   `zoneId` и `spaceId`.
