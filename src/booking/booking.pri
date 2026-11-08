# Подключение модуля бронирования (#9) в общий .pro интегратором:
#   include(src/booking/booking.pri)
# Требует src/core/sessioncontext.h из #2 и QT += sql.
QT += sql
INCLUDEPATH += $$PWD/..
HEADERS += $$PWD/bookingservice.h
SOURCES += $$PWD/bookingservice.cpp
