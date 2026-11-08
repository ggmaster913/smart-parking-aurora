TARGET = ru.iiexperts.SmartParking

CONFIG += \
    auroraapp

QT += sql positioning

include(src/core/core.pri)
include(src/booking/booking.pri)
include(src/history/history.pri)

INCLUDEPATH += $$PWD/src $$PWD/src/core

SOURCES += \
    src/main.cpp \
    src/core/catalogseed.cpp \
    src/account/accountservice.cpp \
    src/account/accountvalidation.cpp \
    src/parking/parkingcatalog.cpp \
    src/parking/parkinggeo.cpp \
    src/parking/parkingservice.cpp

HEADERS += \
    src/core/catalogseed.h \
    src/account/accountservice.h \
    src/account/accountvalidation.h \
    src/parking/parkingcatalog.h \
    src/parking/parkinggeo.h \
    src/parking/parkingservice.h

# Демо-каталог для однократного импорта (CatalogSeed).
data_install.files = data/parking-zones.json
data_install.path = /usr/share/$${TARGET}/data

# MFW (MapTemplate, BSD-3-Clause): QML-плагин MfwMap под архитектуру сборки,
# путь импорта lib/qmlplugins/ добавляется в main.cpp. См. third_party/mfw/README.md.
GCC_INFO = $$system($$QMAKE_CXX -dumpmachine)
contains(GCC_INFO, .*aarch64.*) {
    MFW_LIB_FOLDER = aarch64
} else {
    contains(GCC_INFO, .*armv7hl.*) {
        MFW_LIB_FOLDER = armv7hl
    } else {
        MFW_LIB_FOLDER = x86_64
    }
}
mfw_install.files = \
    third_party/mfw/ru.auroraos/qml/$${MFW_LIB_FOLDER}/MfwMap/qmldir \
    third_party/mfw/ru.auroraos/qml/$${MFW_LIB_FOLDER}/MfwMap/libMfwMap.so
mfw_install.path = /usr/share/$${TARGET}/lib/qmlplugins/MfwMap

INSTALLS += data_install mfw_install

DISTFILES += \
    rpm/ru.iiexperts.SmartParking.spec \

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += auroraapp_i18n

TRANSLATIONS += \
    translations/ru.iiexperts.SmartParking.ts \
    translations/ru.iiexperts.SmartParking-ru.ts \
