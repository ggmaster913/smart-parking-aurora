# Хостовые проверки правил подбора (#20). Не входит в сборку приложения.
# Сборка: qmake && make && ./tst_recommendationrules ; эталон: python3 test_rules.py
TEMPLATE = app
TARGET = tst_recommendationrules
QT += testlib
QT -= gui
CONFIG += console c++14 testcase
CONFIG -= app_bundle

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/src

HEADERS += $$REPO_ROOT/src/experience/recommendationrules.h
SOURCES += \
    $$PWD/tst_recommendationrules.cpp \
    $$REPO_ROOT/src/experience/recommendationrules.cpp

DEFINES += EXPERIENCE_TEST_DIR=\\\"$$PWD\\\"
