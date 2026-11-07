TARGET = ru.iiexperts.SmartParking

CONFIG += \
    auroraapp

PKGCONFIG += \

SOURCES += \
    src/main.cpp \

HEADERS += \

DISTFILES += \
    rpm/ru.iiexperts.SmartParking.spec \

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += auroraapp_i18n

TRANSLATIONS += \
    translations/ru.iiexperts.SmartParking.ts \
    translations/ru.iiexperts.SmartParking-ru.ts \
