TEMPLATE = app
TARGET = tst_startupretry
QT += testlib qml quick
CONFIG += console c++11 testcase
CONFIG -= app_bundle
SOURCES += tst_startupretry.cpp
APP_ROOT = $$clean_path($$PWD/../../..)
DEFINES += APP_TEST_ROOT=\\\"$$APP_ROOT\\\"
