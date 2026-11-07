# Хостовые проверки BookingService (#9). Не входит в сборку приложения.
# Сборка: qmake && make && ./tst_bookingservice  (см. run_host_tests.sh)
TEMPLATE = app
TARGET = tst_bookingservice
QT += testlib sql
QT -= gui
CONFIG += console c++14 testcase
CONFIG -= app_bundle

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/src

# Настоящий SessionContext из #2, если он уже в ветке; иначе тестовая заглушка.
exists($$REPO_ROOT/src/core/sessioncontext.h) {
    HEADERS += $$REPO_ROOT/src/core/sessioncontext.h
    exists($$REPO_ROOT/src/core/sessioncontext.cpp): SOURCES += $$REPO_ROOT/src/core/sessioncontext.cpp
} else {
    INCLUDEPATH += $$PWD/stub
    HEADERS += $$PWD/stub/core/sessioncontext.h
}

HEADERS += $$REPO_ROOT/src/booking/bookingservice.h
SOURCES += \
    $$PWD/tst_bookingservice.cpp \
    $$REPO_ROOT/src/booking/bookingservice.cpp

DEFINES += BOOKING_SOURCE_ROOT=\\\"$$REPO_ROOT\\\"
