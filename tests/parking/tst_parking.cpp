// SPDX-License-Identifier: MIT
//
// Проверки модуля парковок: геометрия, разбор каталога и ParkingService
// на реальной SQLite в памяти. Схема ниже повторяет таблицы zones/spaces/sessions
// из migrations/001_initial.sql (PR #6) в объёме, нужном модулю.
// Эти проверки не доказывают сборку в Aurora SDK.

#include "parkingcatalog.h"
#include "parkinggeo.h"
#include "parkingservice.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

#include <limits>

namespace {

const qint64 kNow = 1794081600; // 2026-11-07T21:00:00Z

class TestParkingService : public ParkingService
{
public:
    TestParkingService(QSqlDatabase db) : ParkingService(db, nullptr) {}
    qint64 now = kNow;

protected:
    qint64 nowUtc() const override { return now; }
};

QString fixturePath()
{
    return QStringLiteral(PARKING_SOURCE_ROOT "/data/parking-zones.json");
}

bool exec(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qWarning() << sql << query.lastError().text();
        return false;
    }
    return true;
}

QStringList ids(const QVariantList &zones)
{
    QStringList result;
    for (const QVariant &zone : zones)
        result << zone.toMap().value(QStringLiteral("id")).toString();
    return result;
}

QVariantMap zoneById(const QVariantList &zones, const QString &id)
{
    for (const QVariant &zone : zones) {
        if (zone.toMap().value(QStringLiteral("id")).toString() == id)
            return zone.toMap();
    }
    return QVariantMap();
}

QByteArray fixtureWith(const QByteArray &zonesJson)
{
    return QByteArray("{\"schemaVersion\":1,\"isDemo\":true,\"zones\":") + zonesJson + "}";
}

const QByteArray kValidZone =
        "{\"id\":\"z1\",\"name\":\"Демо\",\"address\":\"Адрес\",\"latitude\":55.0,"
        "\"longitude\":37.0,\"rateKopecksPerHour\":100,\"workingHours\":\"Круглосуточно\","
        "\"spaces\":[{\"id\":\"z1-1\",\"label\":\"1\",\"occupied\":false}]}";

} // namespace

class ParkingTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // ParkingGeo
    void coordinateValidation_data();
    void coordinateValidation();
    void distanceKnownValues();
    void sortWithoutPositionById();
    void sortByDistanceThenIdUnknownLast();

    // ParkingCatalog
    void realFixtureParses();
    void catalogRejects_data();
    void catalogRejects();

    // ParkingService
    void zonesWithoutPositionSortedById();
    void setPositionRejectsInvalid_data();
    void setPositionRejectsInvalid();
    void setPositionSortsByDistance();
    void demoFlagIsReported();
    void capacityAndFreeSpaces();
    void fullZoneHasNoFreeSpaces();
    void activeSessionMakesSpaceUnavailable();
    void expiredOrCompletedSessionFreesSpace();
    void zoneDetailsResults();
    void sqlCharactersInIdAreHarmless();
    void spacesUnknownZoneEmpty();
    void closedDatabaseKeepsLastCatalog();
    void positionFixRereadsAvailabilityAfterExpiry();
    void expiryTimerScheduledForNearestSession();
    void expiryTimerRefreshesCatalog();

    // PositionTracker.qml (реальный QML с реальным ParkingService)
    void trackerFixThenSignalLost();
    void trackerFixThenAccessDenied();
    void trackerFixGoesStale();
    void trackerDemoNotOverriddenByRealFix();

private:
    void importFixture();

    QSqlDatabase m_db;
    TestParkingService *m_service = nullptr;
};

void ParkingTest::init()
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("parking-test"));
    m_db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(m_db.open());
    QVERIFY(exec(m_db, QStringLiteral("PRAGMA foreign_keys = ON")));
    QVERIFY(exec(m_db, QStringLiteral(
        "CREATE TABLE zones (id TEXT PRIMARY KEY NOT NULL, name TEXT NOT NULL,"
        " address TEXT NOT NULL,"
        " latitude REAL NOT NULL CHECK (latitude BETWEEN -90 AND 90),"
        " longitude REAL NOT NULL CHECK (longitude BETWEEN -180 AND 180),"
        " rate_kopecks_per_hour INTEGER NOT NULL CHECK (rate_kopecks_per_hour >= 0),"
        " working_hours TEXT NOT NULL)")));
    QVERIFY(exec(m_db, QStringLiteral(
        "CREATE TABLE spaces (id TEXT PRIMARY KEY NOT NULL,"
        " zone_id TEXT NOT NULL REFERENCES zones(id), label TEXT NOT NULL,"
        " mock_occupied INTEGER NOT NULL DEFAULT 0 CHECK (mock_occupied IN (0, 1)),"
        " UNIQUE (zone_id, label))")));
    QVERIFY(exec(m_db, QStringLiteral(
        "CREATE TABLE sessions (id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL,"
        " vehicle_id TEXT NOT NULL, space_id TEXT NOT NULL REFERENCES spaces(id),"
        " starts_at_utc INTEGER NOT NULL, expires_at_utc INTEGER NOT NULL,"
        " rate_kopecks_per_hour INTEGER NOT NULL, amount_kopecks INTEGER NOT NULL,"
        " status TEXT NOT NULL CHECK (status IN ('active', 'completed')),"
        " plate_snapshot TEXT NOT NULL)")));
    QVERIFY(exec(m_db, QStringLiteral(
        "CREATE UNIQUE INDEX sessions_one_active_per_space ON sessions(space_id)"
        " WHERE status = 'active'")));
    importFixture();
    m_service = new TestParkingService(m_db);
    m_service->refresh();
}

void ParkingTest::cleanup()
{
    delete m_service;
    m_service = nullptr;
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(QStringLiteral("parking-test"));
}

void ParkingTest::importFixture()
{
    QString error;
    const QVariantList zones = ParkingCatalog::loadFile(fixturePath(), &error);
    QVERIFY2(!zones.isEmpty(), qPrintable(error));
    QVERIFY(m_db.transaction());
    for (const QVariant &zoneValue : zones) {
        const QVariantMap zone = zoneValue.toMap();
        QSqlQuery insertZone(m_db);
        insertZone.prepare(QStringLiteral(
            "INSERT INTO zones VALUES (:id, :name, :address, :lat, :lon, :rate, :hours)"));
        insertZone.bindValue(QStringLiteral(":id"), zone.value(QStringLiteral("id")));
        insertZone.bindValue(QStringLiteral(":name"), zone.value(QStringLiteral("name")));
        insertZone.bindValue(QStringLiteral(":address"), zone.value(QStringLiteral("address")));
        insertZone.bindValue(QStringLiteral(":lat"), zone.value(QStringLiteral("latitude")));
        insertZone.bindValue(QStringLiteral(":lon"), zone.value(QStringLiteral("longitude")));
        insertZone.bindValue(QStringLiteral(":rate"), zone.value(QStringLiteral("rateKopecksPerHour")));
        insertZone.bindValue(QStringLiteral(":hours"), zone.value(QStringLiteral("workingHours")));
        QVERIFY2(insertZone.exec(), qPrintable(insertZone.lastError().text()));
        for (const QVariant &spaceValue : zone.value(QStringLiteral("spaces")).toList()) {
            const QVariantMap space = spaceValue.toMap();
            QSqlQuery insertSpace(m_db);
            insertSpace.prepare(QStringLiteral(
                "INSERT INTO spaces VALUES (:id, :zone, :label, :occupied)"));
            insertSpace.bindValue(QStringLiteral(":id"), space.value(QStringLiteral("id")));
            insertSpace.bindValue(QStringLiteral(":zone"), zone.value(QStringLiteral("id")));
            insertSpace.bindValue(QStringLiteral(":label"), space.value(QStringLiteral("label")));
            insertSpace.bindValue(QStringLiteral(":occupied"),
                                  space.value(QStringLiteral("occupied")).toBool() ? 1 : 0);
            QVERIFY2(insertSpace.exec(), qPrintable(insertSpace.lastError().text()));
        }
    }
    QVERIFY(m_db.commit());
}

// ---------- ParkingGeo ----------

void ParkingTest::coordinateValidation_data()
{
    QTest::addColumn<double>("latitude");
    QTest::addColumn<double>("longitude");
    QTest::addColumn<bool>("valid");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    QTest::newRow("moscow") << 55.75 << 37.61 << true;
    QTest::newRow("bounds") << -90.0 << 180.0 << true;
    QTest::newRow("bounds2") << 90.0 << -180.0 << true;
    QTest::newRow("lat>90") << 90.0001 << 0.0 << false;
    QTest::newRow("lat<-90") << -91.0 << 0.0 << false;
    QTest::newRow("lon>180") << 0.0 << 180.5 << false;
    QTest::newRow("lon<-180") << 0.0 << -181.0 << false;
    QTest::newRow("nan") << nan << 0.0 << false;
    QTest::newRow("inf") << 0.0 << inf << false;
}

void ParkingTest::coordinateValidation()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);
    QFETCH(bool, valid);
    QCOMPARE(ParkingGeo::isValidCoordinate(latitude, longitude), valid);
}

void ParkingTest::distanceKnownValues()
{
    QCOMPARE(ParkingGeo::distanceMeters(55.75, 37.61, 55.75, 37.61), qint64(0));
    // Один градус по экватору при R = 6371008.8 м.
    QCOMPARE(ParkingGeo::distanceMeters(0, 0, 0, 1), qint64(111195));
    // Симметрия.
    QCOMPARE(ParkingGeo::distanceMeters(55.7521, 37.6177, 55.7570, 37.6320),
             ParkingGeo::distanceMeters(55.7570, 37.6320, 55.7521, 37.6177));
    // Примерно 1,04 км между двумя демо-зонами.
    const qint64 d = ParkingGeo::distanceMeters(55.7521, 37.6177, 55.7570, 37.6320);
    QVERIFY2(d > 1000 && d < 1100, qPrintable(QString::number(d)));
    QCOMPARE(ParkingGeo::distanceMeters(91, 0, 0, 0), qint64(-1));
}

void ParkingTest::sortWithoutPositionById()
{
    QVariantList zones;
    for (const char *id : {"c", "a", "b"}) {
        QVariantMap zone;
        zone.insert(QStringLiteral("id"), QString::fromLatin1(id));
        zone.insert(QStringLiteral("distanceMeters"), -1);
        zones << zone;
    }
    ParkingGeo::sortZones(zones);
    QCOMPARE(ids(zones), QStringList() << "a" << "b" << "c");
}

void ParkingTest::sortByDistanceThenIdUnknownLast()
{
    QVariantList zones;
    const QList<QPair<QString, qint64>> rows = {
        {"d", -1}, {"c", 500}, {"b", 100}, {"a", 500}, {"e", 0}};
    for (const auto &row : rows) {
        QVariantMap zone;
        zone.insert(QStringLiteral("id"), row.first);
        zone.insert(QStringLiteral("distanceMeters"), row.second);
        zones << zone;
    }
    ParkingGeo::sortZones(zones);
    QCOMPARE(ids(zones), QStringList() << "e" << "b" << "a" << "c" << "d");
}

// ---------- ParkingCatalog ----------

void ParkingTest::realFixtureParses()
{
    QString error;
    const QVariantList zones = ParkingCatalog::loadFile(fixturePath(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(zones.size() >= 5);
}

void ParkingTest::catalogRejects_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::newRow("not json") << QByteArray("{ broken");
    QTest::newRow("array root") << QByteArray("[]");
    QTest::newRow("schema 2") << QByteArray("{\"schemaVersion\":2,\"isDemo\":true,\"zones\":[")
                                 + kValidZone + "]}";
    QTest::newRow("not demo") << QByteArray("{\"schemaVersion\":1,\"isDemo\":false,\"zones\":[")
                                 + kValidZone + "]}";
    QTest::newRow("no zones") << fixtureWith("[]");
    QByteArray zone = kValidZone;
    QTest::newRow("duplicate zone") << fixtureWith("[" + kValidZone + "," + kValidZone + "]");
    QTest::newRow("bad latitude") << fixtureWith(
        "[" + QByteArray(kValidZone).replace("55.0", "95.0") + "]");
    QTest::newRow("fraction rate") << fixtureWith(
        "[" + QByteArray(kValidZone).replace(":100,", ":100.5,") + "]");
    QTest::newRow("negative rate") << fixtureWith(
        "[" + QByteArray(kValidZone).replace(":100,", ":-1,") + "]");
    QTest::newRow("string rate") << fixtureWith(
        "[" + QByteArray(kValidZone).replace(":100,", ":\"100\",") + "]");
    QTest::newRow("empty spaces") << fixtureWith(
        "[" + QByteArray(kValidZone).replace(
            "[{\"id\":\"z1-1\",\"label\":\"1\",\"occupied\":false}]", "[]") + "]");
    QTest::newRow("occupied not bool") << fixtureWith(
        "[" + QByteArray(kValidZone).replace("\"occupied\":false", "\"occupied\":0") + "]");
    QTest::newRow("empty name") << fixtureWith(
        "[" + QByteArray(kValidZone).replace("\"name\":\"Демо\"", "\"name\":\" \"") + "]");
    QTest::newRow("missing address") << fixtureWith(
        "[" + QByteArray(kValidZone).replace("\"address\":\"Адрес\",", "") + "]");
    Q_UNUSED(zone)
}

void ParkingTest::catalogRejects()
{
    QFETCH(QByteArray, json);
    QString error;
    const QVariantList zones = ParkingCatalog::parse(json, &error);
    QVERIFY(zones.isEmpty());
    QVERIFY(!error.isEmpty());
}

// ---------- ParkingService ----------

void ParkingTest::zonesWithoutPositionSortedById()
{
    QVERIFY(!m_service->hasPosition());
    const QVariantList zones = m_service->zones();
    QCOMPARE(zones.size(), 6);
    QStringList expected = ids(zones);
    expected.sort();
    QCOMPARE(ids(zones), expected);
    for (const QVariant &zone : zones)
        QCOMPARE(zone.toMap().value(QStringLiteral("distanceMeters")).toLongLong(), qint64(-1));
}

void ParkingTest::setPositionRejectsInvalid_data()
{
    coordinateValidation_data();
}

void ParkingTest::setPositionRejectsInvalid()
{
    QFETCH(double, latitude);
    QFETCH(double, longitude);
    QFETCH(bool, valid);
    if (valid)
        QSKIP("валидные координаты проверяются отдельно");
    QSignalSpy spy(m_service, SIGNAL(positionChanged()));
    const QVariantMap result = m_service->setPosition(latitude, longitude, false);
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("errorCode")).toString(), QStringLiteral("INVALID_INPUT"));
    QVERIFY(result.value(QStringLiteral("data")).toMap().isEmpty());
    QVERIFY(!m_service->hasPosition());
    QCOMPARE(spy.count(), 0);
}

void ParkingTest::setPositionSortsByDistance()
{
    QSignalSpy zonesSpy(m_service, SIGNAL(zonesChanged()));
    // Рядом с «Восток».
    const QVariantMap result = m_service->setPosition(55.7480, 37.6600, false);
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(zonesSpy.count(), 1);
    const QVariantList zones = m_service->zones();
    QCOMPARE(zones.first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("demo-east"));
    qint64 previous = -1;
    for (const QVariant &zone : zones) {
        const qint64 distance = zone.toMap().value(QStringLiteral("distanceMeters")).toLongLong();
        QVERIFY(distance >= 0);
        QVERIFY(distance >= previous);
        previous = distance;
    }
    // Повтор той же позиции не шлёт лишних сигналов.
    m_service->setPosition(55.7480, 37.6600, false);
    QCOMPARE(zonesSpy.count(), 1);
}

void ParkingTest::demoFlagIsReported()
{
    QSignalSpy spy(m_service, SIGNAL(positionChanged()));
    QVERIFY(m_service->setPosition(55.7521, 37.6177, true).value(QStringLiteral("ok")).toBool());
    QVERIFY(m_service->hasPosition());
    QVERIFY(m_service->positionIsDemo());
    QVERIFY(m_service->setPosition(55.7521, 37.6177, false).value(QStringLiteral("ok")).toBool());
    QVERIFY(!m_service->positionIsDemo());
    QCOMPARE(spy.count(), 2);
}

void ParkingTest::capacityAndFreeSpaces()
{
    const QVariantMap center = zoneById(m_service->zones(), QStringLiteral("demo-center"));
    QCOMPARE(center.value(QStringLiteral("capacity")).toInt(), 6);
    QCOMPARE(center.value(QStringLiteral("freeSpaces")).toInt(), 4);
    QCOMPARE(center.value(QStringLiteral("rateKopecksPerHour")).toLongLong(), qint64(18000));
    QCOMPARE(center.value(QStringLiteral("workingHours")).toString(), QStringLiteral("Круглосуточно"));
    const QVariantMap freeZone = zoneById(m_service->zones(), QStringLiteral("demo-free-park"));
    QCOMPARE(freeZone.value(QStringLiteral("rateKopecksPerHour")).toLongLong(), qint64(0));
    QVERIFY(freeZone.value(QStringLiteral("freeSpaces")).toInt() > 0);
}

void ParkingTest::fullZoneHasNoFreeSpaces()
{
    const QVariantMap full = zoneById(m_service->zones(), QStringLiteral("demo-full"));
    QCOMPARE(full.value(QStringLiteral("freeSpaces")).toInt(), 0);
    for (const QVariant &space : m_service->spaces(QStringLiteral("demo-full")))
        QCOMPARE(space.toMap().value(QStringLiteral("available")).toBool(), false);
}

void ParkingTest::activeSessionMakesSpaceUnavailable()
{
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s1','u1','v1','demo-center-01',%1,%2,18000,4500,"
        "'active','A123BC')").arg(kNow - 60).arg(kNow + 900)));
    m_service->refresh();
    const QVariantMap center = zoneById(m_service->zones(), QStringLiteral("demo-center"));
    QCOMPARE(center.value(QStringLiteral("freeSpaces")).toInt(), 3);
    bool found = false;
    for (const QVariant &value : m_service->spaces(QStringLiteral("demo-center"))) {
        const QVariantMap space = value.toMap();
        QCOMPARE(space.value(QStringLiteral("zoneId")).toString(), QStringLiteral("demo-center"));
        if (space.value(QStringLiteral("id")).toString() == QLatin1String("demo-center-01")) {
            found = true;
            QCOMPARE(space.value(QStringLiteral("available")).toBool(), false);
        }
    }
    QVERIFY(found);
}

void ParkingTest::expiredOrCompletedSessionFreesSpace()
{
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s1','u1','v1','demo-center-01',%1,%2,18000,4500,"
        "'active','A123BC')").arg(kNow - 3600).arg(kNow)));
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s2','u2','v2','demo-center-03',%1,%2,18000,4500,"
        "'completed','B456CD')").arg(kNow - 60).arg(kNow + 900)));
    m_service->refresh();
    const QVariantMap center = zoneById(m_service->zones(), QStringLiteral("demo-center"));
    QCOMPARE(center.value(QStringLiteral("freeSpaces")).toInt(), 4);

    // Та же активная сессия до истечения занимает место.
    m_service->now = kNow - 1;
    m_service->refresh();
    QCOMPARE(zoneById(m_service->zones(), QStringLiteral("demo-center"))
             .value(QStringLiteral("freeSpaces")).toInt(), 3);
}

void ParkingTest::zoneDetailsResults()
{
    QVariantMap result = m_service->zoneDetails(QString());
    QCOMPARE(result.value(QStringLiteral("errorCode")).toString(), QStringLiteral("INVALID_INPUT"));
    result = m_service->zoneDetails(QStringLiteral("no-such-zone"));
    QCOMPARE(result.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(result.value(QStringLiteral("errorCode")).toString(), QStringLiteral("NOT_FOUND"));
    QVERIFY(!result.value(QStringLiteral("message")).toString().isEmpty());

    result = m_service->zoneDetails(QStringLiteral("  demo-river "));
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    const QVariantMap data = result.value(QStringLiteral("data")).toMap();
    for (const char *key : {"id", "name", "address", "latitude", "longitude",
                            "rateKopecksPerHour", "workingHours", "capacity",
                            "freeSpaces", "distanceMeters"}) {
        QVERIFY2(data.contains(QLatin1String(key)), key);
    }
    QCOMPARE(data.value(QStringLiteral("distanceMeters")).toLongLong(), qint64(-1));

    m_service->setPosition(55.7405, 37.6290, true);
    result = m_service->zoneDetails(QStringLiteral("demo-river"));
    QCOMPARE(result.value(QStringLiteral("data")).toMap()
             .value(QStringLiteral("distanceMeters")).toLongLong(), qint64(0));
}

void ParkingTest::sqlCharactersInIdAreHarmless()
{
    const QString evil = QStringLiteral("demo-center' OR '1'='1; DROP TABLE zones; --");
    QCOMPARE(m_service->zoneDetails(evil).value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("NOT_FOUND"));
    QVERIFY(m_service->spaces(evil).isEmpty());
    m_service->refresh();
    QCOMPARE(m_service->zones().size(), 6);
}

void ParkingTest::spacesUnknownZoneEmpty()
{
    QVERIFY(m_service->spaces(QStringLiteral("nope")).isEmpty());
    QVERIFY(m_service->spaces(QString()).isEmpty());
    QCOMPARE(m_service->spaces(QStringLiteral("demo-river")).size(), 5);
}

void ParkingTest::closedDatabaseKeepsLastCatalog()
{
    m_db.close();
    m_service->refresh();
    QCOMPARE(m_service->zones().size(), 6);
    QCOMPARE(m_service->zoneDetails(QStringLiteral("demo-center"))
             .value(QStringLiteral("errorCode")).toString(), QStringLiteral("STORAGE_ERROR"));
}

namespace {
int freeIn(const QVariantList &zones, const char *zoneId)
{
    return zoneById(zones, QLatin1String(zoneId)).value(QStringLiteral("freeSpaces")).toInt();
}
} // namespace

void ParkingTest::positionFixRereadsAvailabilityAfterExpiry()
{
    // Сессия активна: в зоне 3 свободных места из 4 возможных.
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s1','u1','v1','demo-center-01',%1,%2,18000,4500,"
        "'active','A123BC')").arg(kNow - 60).arg(kNow + 60)));
    m_service->refresh();
    QCOMPARE(freeIn(m_service->zones(), "demo-center"), 3);

    // Сессия истекла, явного refresh() нет, пришёл новый GPS fix.
    m_service->now = kNow + 61;
    QVERIFY(m_service->setPosition(55.7521, 37.6177, false).value(QStringLiteral("ok")).toBool());
    const int listFree = freeIn(m_service->zones(), "demo-center");
    const int cardFree = m_service->zoneDetails(QStringLiteral("demo-center"))
            .value(QStringLiteral("data")).toMap().value(QStringLiteral("freeSpaces")).toInt();
    QCOMPARE(listFree, 4);
    QCOMPARE(cardFree, listFree);
}

void ParkingTest::expiryTimerScheduledForNearestSession()
{
    QCOMPARE(m_service->nextAvailabilityChangeUtc(), qint64(-1));
    QCOMPARE(m_service->scheduledRefreshMs(), -1);

    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s1','u1','v1','demo-center-01',%1,%2,18000,4500,"
        "'active','A123BC')").arg(kNow - 60).arg(kNow + 900)));
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s2','u2','v2','demo-river-01',%1,%2,12000,2000,"
        "'active','B456CD')").arg(kNow - 60).arg(kNow + 300)));
    // Завершённая и уже истёкшая сессии не планируют обновление.
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s3','u3','v3','demo-east-01',%1,%2,9000,2000,"
        "'completed','C789EF')").arg(kNow - 60).arg(kNow + 10)));
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s4','u4','v4','demo-east-02',%1,%2,9000,2000,"
        "'active','D012GH')").arg(kNow - 600).arg(kNow - 5)));
    m_service->refresh();
    QCOMPARE(m_service->nextAvailabilityChangeUtc(), kNow + 300);
    QCOMPARE(m_service->scheduledRefreshMs(), 300 * 1000 + 250);
}

void ParkingTest::expiryTimerRefreshesCatalog()
{
    QVERIFY(exec(m_db, QStringLiteral(
        "INSERT INTO sessions VALUES ('s1','u1','v1','demo-center-01',%1,%2,18000,4500,"
        "'active','A123BC')").arg(kNow - 60).arg(kNow + 1)));
    m_service->refresh();
    QCOMPARE(freeIn(m_service->zones(), "demo-center"), 3);
    QSignalSpy spy(m_service, SIGNAL(zonesChanged()));

    // Часы ушли за срок; обновление должно прийти само, без вызова refresh().
    m_service->now = kNow + 2;
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
    QCOMPARE(freeIn(m_service->zones(), "demo-center"), 4);
    QCOMPARE(m_service->scheduledRefreshMs(), -1);
}

namespace {
QObject *createTracker(QQmlEngine &engine, ParkingService *service)
{
    engine.rootContext()->setContextProperty(QStringLiteral("parkingService"), service);
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QStringLiteral(PARKING_SOURCE_ROOT "/qml/parking/PositionTracker.qml")));
    QObject *tracker = component.create();
    if (!tracker)
        qWarning() << component.errors();
    else
        tracker->setProperty("active", false); // без реального источника на хосте
    return tracker;
}

QString status(QObject *tracker)
{
    return tracker->property("status").toString();
}
} // namespace

void ParkingTest::trackerFixThenSignalLost()
{
    QQmlEngine engine;
    QScopedPointer<QObject> tracker(createTracker(engine, m_service));
    QVERIFY(tracker);
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "handleFix",
                                      Q_ARG(QVariant, 55.7480), Q_ARG(QVariant, 37.6600)));
    QCOMPARE(status(tracker.data()), QStringLiteral("ok"));
    QVERIFY(m_service->hasPosition());
    QVERIFY(!m_service->positionIsDemo());

    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "markProblem", Q_ARG(QVariant, "unavailable")));
    QCOMPARE(status(tracker.data()), QStringLiteral("lost"));
    QVERIFY(tracker->property("statusText").toString().contains(QStringLiteral("последнего известного")));
    // Координата сохранена — расстояния считаются от неё, но состояние видно.
    QVERIFY(m_service->hasPosition());
    QVERIFY(!tracker->property("hasRealFix").toBool());

    // Сигнал вернулся.
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "handleFix",
                                      Q_ARG(QVariant, 55.7481), Q_ARG(QVariant, 37.6601)));
    QCOMPARE(status(tracker.data()), QStringLiteral("ok"));
}

void ParkingTest::trackerFixThenAccessDenied()
{
    QQmlEngine engine;
    QScopedPointer<QObject> tracker(createTracker(engine, m_service));
    QVERIFY(tracker);
    // Без fix запрет доступа — обычное «нет доступа».
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "markProblem", Q_ARG(QVariant, "denied")));
    QCOMPARE(status(tracker.data()), QStringLiteral("denied"));
    QVERIFY(tracker->property("statusText").toString().contains(QStringLiteral("Нет доступа")));

    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "handleFix",
                                      Q_ARG(QVariant, 55.75), Q_ARG(QVariant, 37.61)));
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "markProblem", Q_ARG(QVariant, "denied")));
    QCOMPARE(status(tracker.data()), QStringLiteral("denied"));
    QVERIFY(tracker->property("statusText").toString().contains(QStringLiteral("отозван")));
}

void ParkingTest::trackerFixGoesStale()
{
    QQmlEngine engine;
    QScopedPointer<QObject> tracker(createTracker(engine, m_service));
    QVERIFY(tracker);
    tracker->setProperty("staleAfterMs", 100);
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "handleFix",
                                      Q_ARG(QVariant, 55.75), Q_ARG(QVariant, 37.61)));
    QCOMPARE(status(tracker.data()), QStringLiteral("ok"));
    QTRY_COMPARE_WITH_TIMEOUT(status(tracker.data()), QStringLiteral("lost"), 2000);
}

void ParkingTest::trackerDemoNotOverriddenByRealFix()
{
    QQmlEngine engine;
    QScopedPointer<QObject> tracker(createTracker(engine, m_service));
    QVERIFY(tracker);
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "enableDemo"));
    QVERIFY(m_service->positionIsDemo());
    // Пришла реальная координата — демо-точка в сервисе остаётся.
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "handleFix",
                                      Q_ARG(QVariant, 55.7405), Q_ARG(QVariant, 37.6290)));
    QVERIFY(m_service->positionIsDemo());
    QVERIFY(tracker->property("hasRealFix").toBool());
    // Явный возврат к реальной позиции.
    QVERIFY(QMetaObject::invokeMethod(tracker.data(), "useRealPosition"));
    QVERIFY(!m_service->positionIsDemo());
    QCOMPARE(m_service->zoneDetails(QStringLiteral("demo-river")).value(QStringLiteral("data"))
             .toMap().value(QStringLiteral("distanceMeters")).toLongLong(), qint64(0));
}

QTEST_MAIN(ParkingTest)

#include "tst_parking.moc"
