# SPDX-License-Identifier: MIT
# Хостовые проверки модуля парковок (QtTest + SQLite в памяти).
# Сборка:  cd tests/parking && qmake parking-tests.pro && make && ./tst_parking
# Это не сборка приложения в Aurora SDK.

TEMPLATE = app
TARGET = tst_parking
CONFIG += console testcase c++11
CONFIG -= app_bundle
QT += testlib sql qml quick

PARKING_SRC = $$PWD/../../src/parking
INCLUDEPATH += $$PARKING_SRC
DEFINES += PARKING_SOURCE_ROOT=\\\"$$clean_path($$PWD/../..)\\\"

HEADERS += \
    $$PARKING_SRC/parkingcatalog.h \
    $$PARKING_SRC/parkinggeo.h \
    $$PARKING_SRC/parkingservice.h

SOURCES += \
    $$PARKING_SRC/parkingcatalog.cpp \
    $$PARKING_SRC/parkinggeo.cpp \
    $$PARKING_SRC/parkingservice.cpp \
    tst_parking.cpp
