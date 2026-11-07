# Подключение подбора парковок (#20) в общий .pro интегратором:
#   include(src/experience/experience.pri)
# Требует уже подключённые src/parking и src/booking.
INCLUDEPATH += $$PWD/..
HEADERS += \
    $$PWD/recommendationrules.h \
    $$PWD/parkingrecommendationservice.h
SOURCES += \
    $$PWD/recommendationrules.cpp \
    $$PWD/parkingrecommendationservice.cpp
