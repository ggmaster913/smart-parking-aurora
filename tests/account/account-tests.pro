# SPDX-License-Identifier: MIT
# Хостовые проверки модуля профиля (QtTest + SQLite в памяти).
# Сборка:  cd tests/account && qmake account-tests.pro && make && ./tst_account
# Это не сборка приложения в Aurora SDK.

TEMPLATE = app
TARGET = tst_account
CONFIG += console testcase c++11
CONFIG -= app_bundle
QT += testlib sql
QT -= gui

ACCOUNT_SRC = $$PWD/../../src/account
INCLUDEPATH += $$ACCOUNT_SRC $$PWD/stubs

HEADERS += \
    $$PWD/stubs/sessioncontext.h \
    $$ACCOUNT_SRC/accountservice.h \
    $$ACCOUNT_SRC/accountvalidation.h

SOURCES += \
    $$ACCOUNT_SRC/accountservice.cpp \
    $$ACCOUNT_SRC/accountvalidation.cpp \
    tst_account.cpp
