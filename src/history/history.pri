# Подключение модуля истории (#22) в общий .pro интегратором:
#   include(src/history/history.pri)
# Требует src/core/sessioncontext.h и QT += sql.
QT += sql
INCLUDEPATH += $$PWD/..
HEADERS += $$PWD/historyservice.h
SOURCES += $$PWD/historyservice.cpp
