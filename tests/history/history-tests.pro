# Хостовые проверки HistoryService (#22). Не входит в сборку приложения.
# Сборка: qmake && make && ./tst_historyservice  (см. run_host_tests.sh)
TEMPLATE = app
TARGET = tst_historyservice
QT += testlib sql
QT -= gui
CONFIG += console c++14 testcase
CONFIG -= app_bundle

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/src

HEADERS += \
    $$REPO_ROOT/src/core/sessioncontext.h \
    $$REPO_ROOT/src/booking/bookingservice.h \
    $$REPO_ROOT/src/history/historyservice.h
SOURCES += \
    $$PWD/tst_historyservice.cpp \
    $$REPO_ROOT/src/core/sessioncontext.cpp \
    $$REPO_ROOT/src/booking/bookingservice.cpp \
    $$REPO_ROOT/src/history/historyservice.cpp

DEFINES += HISTORY_SOURCE_ROOT=\\\"$$REPO_ROOT\\\"
