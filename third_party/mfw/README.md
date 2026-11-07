# MFW (карта) — подключение для «Умной парковки»

Каталог предназначен для **только необходимых** файлов библиотеки MfwMap из официального
шаблона [MapTemplate](https://hub.mos.ru/auroraos/demos/MapTemplate) (© 2024 Open Mobile
Platform LLC, лицензия BSD-3-Clause). Маршрутизация (MfwOfflineRouting, protobuf)
и геокодер (MfwOfflineGeocoding) приложению не нужны и не переносятся.

## Происхождение файлов (07.11.2026)

Скопированы без изменений из архива ветки `example` MapTemplate (скачан Ильёй с hub.mos.ru;
README шаблона: «совместим с пятой версией ОС Аврора»). Взят только каталог `MfwMap`:

| Файл | SHA-256 | Тип |
|---|---|---|
| `ru.auroraos/qml/x86_64/MfwMap/libMfwMap.so` | `9f7a072a2940314ef0d9ff6a3a0ba29fdbc1b309679b378c1a2973cbf9797d58` | ELF 64-bit x86-64 (эмулятор) |
| `ru.auroraos/qml/aarch64/MfwMap/libMfwMap.so` | `6fe7eb10960376228eec3432a1d8cea366bed047bafa074925e40b2677de1f7e` | ELF 64-bit ARM aarch64 |
| `ru.auroraos/qml/armv7hl/MfwMap/libMfwMap.so` | `167b5aa5a4596a51872939596dcd191373d0838087292159066dec88fd2cdb24` | ELF 32-bit ARM EABI5 |
| `ru.auroraos/qml/<arch>/MfwMap/qmldir` | — | `module MfwMap` / `plugin MfwMap` |

`LICENSE.BSD-3-Clause.md` и `AUTHORS.md` — из корня MapTemplate. Исходники библиотеки:
https://gitlab.com/omprussia/libraries/mfw-map (по README шаблона). По символам библиотека
построена на QMapLibreGL; отдельных уведомлений о лицензиях сторонних компонентов в архиве
шаблона нет.

Зависимости `libMfwMap.so` (readelf, одинаковы для трёх архитектур): Qt5Quick, Qt5Positioning,
Qt5Svg, Qt5Qml, Qt5Sql, Qt5Network, Qt5Gui, Qt5Core, libz, libstdc++. Версия символов Qt — до
`Qt_5.6`. Spec MapTemplate явных `Requires` на эти пакеты не добавляет.

## Статус

- Подключение MFW **не проверено сборкой**. По решению координатора проба MFW выполняется на
  уже установленном у Ильи **Aurora SDK 5.2.1.200 MB2**; целевая сборка — BT (#1). Успешная проба
  MB2 не считается проверкой BT.

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

3. `.desktop`: в `[X-Application]` — `Permissions=Internet;Location` (тайлы и геопозиция;
   имена подтверждены координатором по документации desktop requirements).

4. `rpm/*.spec`: `Requires: qt5-qtdeclarative-import-positioning` (QML-модуль QtPositioning,
   по [документации](https://developer.auroraos.ru/doc/software_development/guides/cpp_api/positioning)).
   Библиотеки в `/usr/share/<имя>` исключаются из автоматических provides/requires так же,
   как в `rpm/ru.auroraos.MapTemplate.spec`:

   ```spec
   %global __provides_exclude_from ^%{_datadir}/%{name}.*$
   %global __requires_exclude_from ^%{_datadir}/%{name}.*$
   ```

   `.desktop` MapTemplate: `Permissions=Documents;Internet;Location;WebView` — нам из этого
   нужны только `Internet;Location` (подтверждено координатором в #4).

5. Импорт `data/parking-zones.json` в новую БД: `ParkingCatalog::loadFile()` возвращает
   проверенный список зон с местами или пустой список и русскую ошибку — без частичных
   данных. Поле `occupied` → `spaces.mock_occupied` (0/1).

6. Навигация: `qml/parking/ParkingMapPage.qml` — входная страница этапа 2; свойство
   `bookingPageUrl` указывает на страницу подтверждения Альберта, она получает свойства
   `zoneId` и `spaceId`.
