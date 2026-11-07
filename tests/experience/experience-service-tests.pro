# Хостовые проверки ParkingRecommendationService (#20) с настоящими ParkingService и BookingService.
# Не входит в сборку приложения. Сборка: qmake && make && ./tst_recommendationservice
TEMPLATE = app
TARGET = tst_recommendationservice
QT += testlib sql qml quick
CONFIG += console c++14 testcase
CONFIG -= app_bundle

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/src $$REPO_ROOT/src/parking

HEADERS += \
    $$REPO_ROOT/src/core/sessioncontext.h \
    $$REPO_ROOT/src/booking/bookingservice.h \
    $$REPO_ROOT/src/parking/parkingcatalog.h \
    $$REPO_ROOT/src/parking/parkinggeo.h \
    $$REPO_ROOT/src/parking/parkingservice.h \
    $$REPO_ROOT/src/experience/recommendationrules.h \
    $$REPO_ROOT/src/experience/parkingrecommendationservice.h
SOURCES += \
    $$PWD/tst_recommendationservice.cpp \
    $$REPO_ROOT/src/core/sessioncontext.cpp \
    $$REPO_ROOT/src/booking/bookingservice.cpp \
    $$REPO_ROOT/src/parking/parkingcatalog.cpp \
    $$REPO_ROOT/src/parking/parkinggeo.cpp \
    $$REPO_ROOT/src/parking/parkingservice.cpp \
    $$REPO_ROOT/src/experience/recommendationrules.cpp \
    $$REPO_ROOT/src/experience/parkingrecommendationservice.cpp

DEFINES += EXPERIENCE_SOURCE_ROOT=\\\"$$REPO_ROOT\\\"
