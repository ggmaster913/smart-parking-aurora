// Проверки ParkingRecommendationService (#20) с настоящими ParkingService и BookingService
// на SQLite со схемой migrations/001_initial.sql. Хостовые тесты, не сборка в Aurora SDK.
#include <QtTest>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include "booking/bookingservice.h"
#include "core/sessioncontext.h"
#include "experience/parkingrecommendationservice.h"
#include "parking/parkingservice.h"

namespace {

const qint64 StartUtc = 1762538400; // 2025-11-07 18:00:00 UTC

class TestParking : public ParkingService
{
public:
    TestParking(QSqlDatabase db, SessionContext *context, qint64 *clock)
        : ParkingService(db, context), m_clock(clock) {}

protected:
    qint64 nowUtc() const override { return *m_clock; }

private:
    qint64 *m_clock;
};

class TestBooking : public BookingService
{
public:
    TestBooking(QSqlDatabase db, SessionContext *context, qint64 *clock)
        : BookingService(db, context), m_clock(clock) {}

protected:
    qint64 currentUtc() const override { return *m_clock; }

private:
    qint64 *m_clock;
};

bool exec(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << "SQL failed:" << sql << q.lastError().text();
        return false;
    }
    return true;
}

QVariantList recommendations(const QVariantMap &result)
{
    return result.value(QStringLiteral("data")).toMap().value(QStringLiteral("recommendations")).toList();
}

QVariantMap byMode(const QVariantMap &result, const QString &mode)
{
    for (const QVariant &value : recommendations(result)) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("mode")).toString() == mode)
            return item;
    }
    return QVariantMap();
}

QStringList modes(const QVariantMap &result)
{
    QStringList out;
    for (const QVariant &value : recommendations(result))
        out << value.toMap().value(QStringLiteral("mode")).toString();
    return out;
}

} // namespace

class TstRecommendationService : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void invalidDuration();
    void withoutPositionCheapestAndPriceOnlyBalance();
    void withPositionAllModes();
    void amountComesFromQuote();
    void occupiedCatalogGivesEmptyList();
    void bookedLastSpaceDisappearsAfterRefresh();

private:
    QSqlDatabase m_db;
    SessionContext *m_context = nullptr;
    TestParking *m_parking = nullptr;
    TestBooking *m_booking = nullptr;
    ParkingRecommendationService *m_service = nullptr;
    qint64 m_now = StartUtc;
};

void TstRecommendationService::init()
{
    m_now = StartUtc;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QUuid::createUuid().toString());
    m_db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(m_db.open());
    QVERIFY(exec(m_db, QStringLiteral("PRAGMA foreign_keys = ON")));

    QFile file(QStringLiteral(EXPERIENCE_SOURCE_ROOT "/migrations/001_initial.sql"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QString sql = QString::fromUtf8(file.readAll());
    sql.remove(QRegularExpression(QStringLiteral("--[^\n]*")));
    for (const QString &statement : sql.split(QLatin1Char(';'))) {
        if (!statement.trimmed().isEmpty())
            QVERIFY(exec(m_db, statement));
    }

    // near — дорогая рядом с демо-точкой; far — дешёвая дальше; free — бесплатная, всё занято;
    // single — одно свободное место.
    const QStringList fixtures = {
        "INSERT INTO users(id, contact, display_name) VALUES ('u1','u1@demo.test','Первый')",
        "INSERT INTO vehicles(id, user_id, plate, brand) VALUES ('v1','u1','А123ВС77','Лада')",
        "INSERT INTO zones VALUES ('near','Рядом','Адрес 1',55.7558,37.6173,18000,'Круглосуточно')",
        "INSERT INTO zones VALUES ('far','Дальняя','Адрес 2',55.7900,37.6600,6000,'Круглосуточно')",
        "INSERT INTO zones VALUES ('free','Бесплатная','Адрес 3',55.7600,37.6200,0,'Круглосуточно')",
        "INSERT INTO zones VALUES ('single','Одно место','Адрес 4',55.7700,37.6300,12000,'Круглосуточно')",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('near-1','near','01',0)",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('near-2','near','02',0)",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('far-1','far','01',0)",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('free-1','free','01',1)",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('single-1','single','01',0)",
    };
    for (const QString &statement : fixtures)
        QVERIFY(exec(m_db, statement));

    m_context = new SessionContext;
    m_context->setUserId(QStringLiteral("u1"));
    m_parking = new TestParking(m_db, m_context, &m_now);
    m_booking = new TestBooking(m_db, m_context, &m_now);
    m_parking->refresh();
    m_booking->refresh();
    m_service = new ParkingRecommendationService(m_parking, m_booking);
}

void TstRecommendationService::cleanup()
{
    delete m_service;
    m_service = nullptr;
    delete m_booking;
    m_booking = nullptr;
    delete m_parking;
    m_parking = nullptr;
    delete m_context;
    m_context = nullptr;
    const QString name = m_db.connectionName();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

void TstRecommendationService::invalidDuration()
{
    for (int minutes : {0, 20, 1455}) {
        const QVariantMap r = m_service->recommend(minutes);
        QCOMPARE(r.value("ok").toBool(), false);
        QCOMPARE(r.value("errorCode").toString(), QStringLiteral("INVALID_INPUT"));
    }
}

void TstRecommendationService::withoutPositionCheapestAndPriceOnlyBalance()
{
    QVERIFY(!m_parking->hasPosition());
    const QVariantMap r = m_service->recommend(60);
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(r.value("data").toMap().value("positionAvailable").toBool(), false);
    QCOMPARE(modes(r), QStringList({"cheapest", "balanced"}));
    // Бесплатная зона занята целиком и не предлагается; дешевле всех — far (6000/ч).
    QCOMPARE(byMode(r, "cheapest").value("zoneId").toString(), QStringLiteral("far"));
    QCOMPARE(byMode(r, "balanced").value("zoneId").toString(), QStringLiteral("far"));
    QCOMPARE(byMode(r, "balanced").value("reason").toString(),
             QStringLiteral("Геопозиция недоступна — выбрана меньшая стоимость"));
    QCOMPARE(byMode(r, "cheapest").value("distanceMeters").toLongLong(), qint64(-1));
}

void TstRecommendationService::withPositionAllModes()
{
    QVERIFY(m_parking->setPosition(55.7558, 37.6173, true).value("ok").toBool());
    const QVariantMap r = m_service->recommend(60);
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(r.value("data").toMap().value("positionAvailable").toBool(), true);
    QCOMPARE(modes(r), QStringList({"nearest", "cheapest", "balanced"}));
    QCOMPARE(byMode(r, "nearest").value("zoneId").toString(), QStringLiteral("near"));
    QCOMPARE(byMode(r, "cheapest").value("zoneId").toString(), QStringLiteral("far"));
    QVERIFY(byMode(r, "nearest").value("distanceMeters").toLongLong() >= 0);
    for (const QVariant &value : recommendations(r)) {
        const QVariantMap item = value.toMap();
        QVERIFY(item.value("zoneId").toString() != QLatin1String("free"));
        QCOMPARE(item.value("currency").toString(), QStringLiteral("RUB"));
        QVERIFY(item.value("freeSpaces").toInt() > 0);
        QVERIFY(!item.value("reason").toString().isEmpty());
    }
}

void TstRecommendationService::amountComesFromQuote()
{
    QVERIFY(m_parking->setPosition(55.7558, 37.6173, true).value("ok").toBool());
    const QVariantMap r = m_service->recommend(45);
    const QVariantMap nearest = byMode(r, "nearest");
    const QVariantMap quote = m_booking->quote(QStringLiteral("near"), 45);
    QCOMPARE(nearest.value("amountKopecks").toLongLong(),
             quote.value("data").toMap().value("amountKopecks").toLongLong());
    QCOMPARE(nearest.value("amountKopecks").toLongLong(), qint64(13500));
}

void TstRecommendationService::occupiedCatalogGivesEmptyList()
{
    QVERIFY(exec(m_db, QStringLiteral("UPDATE spaces SET mock_occupied = 1")));
    m_parking->refresh();
    const QVariantMap r = m_service->recommend(60);
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(recommendations(r).size(), 0);
}

void TstRecommendationService::bookedLastSpaceDisappearsAfterRefresh()
{
    // Оставляем свободной только зону single, затем бронируем её последнее место.
    QVERIFY(exec(m_db, QStringLiteral("UPDATE spaces SET mock_occupied = 1 WHERE zone_id <> 'single'")));
    m_parking->refresh();
    QCOMPARE(byMode(m_service->recommend(60), "cheapest").value("zoneId").toString(), QStringLiteral("single"));

    const QVariantMap booked = m_booking->book(QStringLiteral("single"), QStringLiteral("single-1"),
                                               QStringLiteral("v1"), 60, QStringLiteral("op-rec-1"));
    QVERIFY2(booked.value("ok").toBool(), qPrintable(booked.value("message").toString()));
    m_parking->refresh();
    const QVariantMap after = m_service->recommend(60);
    QVERIFY(after.value("ok").toBool());
    QCOMPARE(recommendations(after).size(), 0);
}

QTEST_MAIN(TstRecommendationService)
#include "tst_recommendationservice.moc"
