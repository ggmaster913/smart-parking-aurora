// Проверки BookingService (#9) на SQLite в памяти со схемой migrations/001_initial.sql.
// Хостовые тесты: подтверждают бизнес-правила и SQL, но не сборку в Aurora SDK.
#include <QtTest>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include "booking/bookingservice.h"
#include "core/sessioncontext.h"

namespace {

class TestBookingService : public BookingService
{
public:
    TestBookingService(QSqlDatabase db, SessionContext *context, qint64 *clock)
        : BookingService(db, context), m_clock(clock) {}

protected:
    qint64 currentUtc() const override { return *m_clock; }

private:
    qint64 *m_clock;
};

const qint64 StartUtc = 1762538400; // 2025-11-07 18:00:00 UTC

QString schemaPath()
{
    const QByteArray env = qgetenv("BOOKING_SCHEMA_SQL");
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env);
    return QStringLiteral(BOOKING_SOURCE_ROOT "/migrations/001_initial.sql");
}

bool execOrFail(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << "SQL failed:" << sql << q.lastError().text();
        return false;
    }
    return true;
}

int count(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return -1;
    return q.value(0).toInt();
}

QVariantMap dataOf(const QVariantMap &result) { return result.value("data").toMap(); }

} // namespace

class TstBookingService : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void price_data();
    void price();
    void quoteUsesZoneRate();
    void quoteFreeZone();
    void quoteUnknownZone();
    void invalidDuration_data();
    void invalidDuration();

    void bookCreatesSessionAndDemoPayment();
    void bookRequiresSignedInUser();
    void bookRejectsForeignAndUnknownVehicle();
    void bookRejectsSpaceOutsideZoneAndOccupied();
    void bookRejectsSpaceTakenByAnotherUser();
    void bookRejectsSecondActiveSessionOfUser();
    void bookFreeZone();
    void bookIsIdempotent();
    void bookReusedKeyConflicts();
    void bookRetryAfterExtendReturnsOriginalResponse();
    void bookRollsBackOnStorageError();
    void sqlInjectionIsInert();

    void extendAddsToStoredExpiryWithStoredRate();
    void extendIsIdempotent();
    void extendRejectsForeignUnknownAndExpired();
    void extendReusedKeyConflicts();

    void amountAboveLimitIsRejected();
    void largeAmountSurvivesReplayExactly();
    void reusedKeyWithInvalidInputConflicts();
    void extendRollsBackOnStorageError();
    void extendRetryAfterExpiryReturnsOriginalResponse();
    void bookExpiredSpaceWithoutRefresh();

    void refreshCompletesExpiredAndFreesSpace();
    void restartRestoresActiveSession();
    void userChangeReloadsActiveSession();

private:
    QVariantMap bookDefault(const QString &op = QStringLiteral("op-book-1"));

    QSqlDatabase m_db;
    SessionContext *m_context = nullptr;
    TestBookingService *m_service = nullptr;
    qint64 m_now = StartUtc;
};

void TstBookingService::init()
{
    const QString name = QUuid::createUuid().toString();
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    m_db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(m_db.open());
    QVERIFY(execOrFail(m_db, QStringLiteral("PRAGMA foreign_keys = ON")));

    QFile file(schemaPath());
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable("Нет схемы: " + schemaPath()));
    QString sql = QString::fromUtf8(file.readAll());
    sql.remove(QRegularExpression(QStringLiteral("--[^\n]*")));
    const QStringList statements = sql.split(QLatin1Char(';'));
    for (const QString &statement : statements) {
        if (!statement.trimmed().isEmpty())
            QVERIFY(execOrFail(m_db, statement));
    }
    QCOMPARE(count(m_db, QStringLiteral("PRAGMA user_version")), 1);

    const QStringList fixtures = {
        "INSERT INTO users(id, contact, display_name) VALUES ('u1','u1@demo.test','Первый')",
        "INSERT INTO users(id, contact, display_name) VALUES ('u2','u2@demo.test','Второй')",
        "INSERT INTO vehicles(id, user_id, plate, brand) VALUES ('v1','u1','А123ВС77','Лада')",
        "INSERT INTO vehicles(id, user_id, plate, brand) VALUES ('v2','u2','В456ОР99','Камаз')",
        "INSERT INTO zones VALUES ('z-paid','Платная','Адрес 1',55.75,37.61,18000,'Круглосуточно')",
        "INSERT INTO zones VALUES ('z-free','Бесплатная','Адрес 2',55.76,37.62,0,'Круглосуточно')",
        "INSERT INTO zones VALUES ('z-odd','Нечётная','Адрес 3',55.77,37.63,101,'Круглосуточно')",
        "INSERT INTO spaces VALUES ('s1','z-paid','01',0)",
        "INSERT INTO spaces VALUES ('s2','z-paid','02',0)",
        "INSERT INTO spaces VALUES ('s3','z-paid','03',1)",
        "INSERT INTO spaces VALUES ('f1','z-free','01',0)",
    };
    for (const QString &statement : fixtures)
        QVERIFY(execOrFail(m_db, statement));

    m_now = StartUtc;
    m_context = new SessionContext;
    m_context->setUserId(QStringLiteral("u1"));
    m_service = new TestBookingService(m_db, m_context, &m_now);
}

void TstBookingService::cleanup()
{
    delete m_service;
    m_service = nullptr;
    delete m_context;
    m_context = nullptr;
    const QString name = m_db.connectionName();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

QVariantMap TstBookingService::bookDefault(const QString &op)
{
    return m_service->book(QStringLiteral("z-paid"), QStringLiteral("s1"),
                           QStringLiteral("v1"), 45, op);
}

void TstBookingService::price_data()
{
    QTest::addColumn<qint64>("rate");
    QTest::addColumn<int>("minutes");
    QTest::addColumn<qint64>("expected");

    QTest::newRow("контракт 45 мин") << qint64(18000) << 45 << qint64(13500);
    QTest::newRow("контракт продление 15") << qint64(18000) << 15 << qint64(4500);
    QTest::newRow("ровно 25.0") << qint64(100) << 15 << qint64(25);
    QTest::newRow("25.25 вверх") << qint64(101) << 15 << qint64(26);
    QTest::newRow("0.25 вверх") << qint64(1) << 15 << qint64(1);
    QTest::newRow("бесплатно") << qint64(0) << 1440 << qint64(0);
    QTest::newRow("сутки") << qint64(18000) << 1440 << qint64(432000);
    QTest::newRow("большой тариф без int32") << qint64(5000000000LL) << 1440
                                             << qint64(120000000000LL);
    QTest::newRow("предел 2^53-1") << qint64(36028797018963964LL) << 15
                                   << qint64(9007199254740991LL);
    QTest::newRow("ровно 2^53") << qint64(36028797018963968LL) << 15 << qint64(-1);
    QTest::newRow("2^53+1 из ревью") << qint64(36028797018963972LL) << 15 << qint64(-1);
    QTest::newRow("отрицательный") << qint64(-1) << 15 << qint64(-1);
    QTest::newRow("переполнение") << qint64(0x7fffffffffffffffLL) << 15 << qint64(-1);
}

void TstBookingService::price()
{
    QFETCH(qint64, rate);
    QFETCH(int, minutes);
    QFETCH(qint64, expected);
    QCOMPARE(BookingService::priceKopecks(rate, minutes), expected);
}

void TstBookingService::quoteUsesZoneRate()
{
    const QVariantMap r = m_service->quote(QStringLiteral("z-paid"), 45);
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(r.value("errorCode").toString(), QString());
    const QVariantMap d = dataOf(r);
    QCOMPARE(d.value("amountKopecks").toLongLong(), 13500LL);
    QCOMPARE(d.value("durationMinutes").toInt(), 45);
    QCOMPARE(d.value("currency").toString(), QStringLiteral("RUB"));

    const QVariantMap odd = m_service->quote(QStringLiteral("z-odd"), 15);
    QCOMPARE(dataOf(odd).value("amountKopecks").toLongLong(), 26LL);
    // quote ничего не бронирует и не оплачивает
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 0);
}

void TstBookingService::quoteFreeZone()
{
    const QVariantMap r = m_service->quote(QStringLiteral("z-free"), 1440);
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(dataOf(r).value("amountKopecks").toLongLong(), 0LL);
}

void TstBookingService::quoteUnknownZone()
{
    const QVariantMap r = m_service->quote(QStringLiteral("нет-такой"), 15);
    QVERIFY(!r.value("ok").toBool());
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("NOT_FOUND"));
    QVERIFY(!r.value("message").toString().isEmpty());
    QVERIFY(dataOf(r).isEmpty());
}

void TstBookingService::invalidDuration_data()
{
    QTest::addColumn<int>("minutes");
    QTest::newRow("0") << 0;
    QTest::newRow("отрицательная") << -15;
    QTest::newRow("14") << 14;
    QTest::newRow("16") << 16;
    QTest::newRow("20") << 20;
    QTest::newRow("1455") << 1455;
    QTest::newRow("огромная") << 2147483647;
}

void TstBookingService::invalidDuration()
{
    QFETCH(int, minutes);
    QVERIFY(!BookingService::isValidDuration(minutes));
    QCOMPARE(m_service->quote(QStringLiteral("z-paid"), minutes).value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(m_service->book("z-paid", "s1", "v1", minutes, "op-bad")
                 .value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);

    QVERIFY(bookDefault().value("ok").toBool());
    const QString sessionId = m_service->activeSession().value("id").toString();
    QCOMPARE(m_service->extend(sessionId, minutes, "op-ext-bad").value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);

    QVERIFY(BookingService::isValidDuration(15));
    QVERIFY(BookingService::isValidDuration(1440));
}

void TstBookingService::bookCreatesSessionAndDemoPayment()
{
    QSignalSpy spy(m_service, SIGNAL(activeSessionChanged()));
    const QVariantMap r = bookDefault();
    QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("message").toString()));
    QCOMPARE(r.value("errorCode").toString(), QString());

    const QVariantMap s = dataOf(r);
    const QString id = s.value("id").toString();
    QVERIFY(!id.isEmpty());
    QCOMPARE(s.value("zoneId").toString(), QStringLiteral("z-paid"));
    QCOMPARE(s.value("spaceId").toString(), QStringLiteral("s1"));
    QCOMPARE(s.value("vehicleId").toString(), QStringLiteral("v1"));
    QCOMPARE(s.value("plate").toString(), QStringLiteral("А123ВС77"));
    QCOMPARE(s.value("startsAtUtc").toLongLong(), StartUtc);
    QCOMPARE(s.value("expiresAtUtc").toLongLong(), StartUtc + 45 * 60);
    QCOMPARE(s.value("amountKopecks").toLongLong(), 13500LL);
    QCOMPARE(s.value("currency").toString(), QStringLiteral("RUB"));
    QCOMPARE(s.value("status").toString(), QStringLiteral("active"));
    QCOMPARE(s.value("ticketNumber").toString(), QStringLiteral("DEMO-") + id);
    QCOMPARE(s.value("paymentIsDemo").toBool(), true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_service->activeSession().value("id").toString(), id);
    QCOMPARE(m_service->activeSession().value("ticketNumber").toString(), "DEMO-" + id);

    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='active'"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations "
                         "WHERE id='op-book-1' AND kind='book' AND amount_kopecks=13500 "
                         "AND user_id='u1'"), 1);
    QCOMPARE(count(m_db, "SELECT rate_kopecks_per_hour FROM sessions"), 18000);
}

void TstBookingService::bookRequiresSignedInUser()
{
    m_context->setUserId(QString());
    QCOMPARE(bookDefault().value("errorCode").toString(), QStringLiteral("NOT_AUTHENTICATED"));

    m_context->setUserId(QStringLiteral("удалённый"));
    QCOMPARE(bookDefault().value("errorCode").toString(), QStringLiteral("NOT_AUTHENTICATED"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 0);
}

void TstBookingService::bookRejectsForeignAndUnknownVehicle()
{
    QVariantMap r = m_service->book("z-paid", "s1", "v2", 45, "op-1");
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("NOT_OWNER"));
    QVERIFY(dataOf(r).isEmpty());
    r = m_service->book("z-paid", "s1", "v-none", 45, "op-2");
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("NOT_FOUND"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);
}

void TstBookingService::bookRejectsSpaceOutsideZoneAndOccupied()
{
    QCOMPARE(m_service->book("z-free", "s1", "v1", 45, "op-1").value("errorCode").toString(),
             QStringLiteral("NOT_FOUND"));
    QCOMPARE(m_service->book("z-paid", "нет", "v1", 45, "op-2").value("errorCode").toString(),
             QStringLiteral("NOT_FOUND"));
    QCOMPARE(m_service->book("z-paid", "s3", "v1", 45, "op-3").value("errorCode").toString(),
             QStringLiteral("SPACE_UNAVAILABLE"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);
}

void TstBookingService::bookRejectsSpaceTakenByAnotherUser()
{
    QVERIFY(bookDefault().value("ok").toBool());
    m_context->setUserId(QStringLiteral("u2"));
    QCOMPARE(m_service->book("z-paid", "s1", "v2", 45, "op-u2").value("errorCode").toString(),
             QStringLiteral("SPACE_UNAVAILABLE"));
    QVERIFY(m_service->book("z-paid", "s2", "v2", 45, "op-u2b").value("ok").toBool());
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='active'"), 2);
}

void TstBookingService::bookRejectsSecondActiveSessionOfUser()
{
    QVERIFY(bookDefault().value("ok").toBool());
    QCOMPARE(m_service->book("z-paid", "s2", "v1", 15, "op-2").value("errorCode").toString(),
             QStringLiteral("ACTIVE_SESSION_EXISTS"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);
}

void TstBookingService::bookFreeZone()
{
    const QVariantMap r = m_service->book("z-free", "f1", "v1", 60, "op-free");
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(dataOf(r).value("amountKopecks").toLongLong(), 0LL);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations WHERE amount_kopecks=0"), 1);
}

void TstBookingService::bookIsIdempotent()
{
    const QVariantMap first = bookDefault();
    QVERIFY(first.value("ok").toBool());
    m_now += 5; // двойное нажатие чуть позже
    const QVariantMap second = bookDefault();
    QCOMPARE(second, first);
    QCOMPARE(dataOf(second).value("amountKopecks").toLongLong(), 13500LL);
    QCOMPARE(dataOf(second).value("startsAtUtc").toLongLong(), StartUtc);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);

    // повтор после «перезапуска» — новый экземпляр сервиса на той же БД
    TestBookingService restarted(m_db, m_context, &m_now);
    QCOMPARE(restarted.book("z-paid", "s1", "v1", 45, "op-book-1"), first);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);
}

void TstBookingService::bookReusedKeyConflicts()
{
    QVERIFY(bookDefault().value("ok").toBool());

    QVariantMap r = m_service->book("z-paid", "s1", "v1", 60, "op-book-1");
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QVERIFY(dataOf(r).isEmpty());
    r = m_service->book("z-paid", "s2", "v1", 45, "op-book-1");
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("IDEMPOTENCY_CONFLICT"));

    // другой пользователь с тем же ключом и теми же параметрами не видит чужой ответ
    m_context->setUserId(QStringLiteral("u2"));
    r = m_service->book("z-paid", "s1", "v1", 45, "op-book-1");
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QVERIFY(dataOf(r).isEmpty());
    QVERIFY(!r.value("message").toString().contains(QStringLiteral("А123ВС77")));

    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);
}

void TstBookingService::bookRetryAfterExtendReturnsOriginalResponse()
{
    const QVariantMap original = bookDefault();
    QVERIFY(original.value("ok").toBool());
    const QString sessionId = dataOf(original).value("id").toString();

    m_now += 600;
    QVERIFY(m_service->extend(sessionId, 15, "op-ext-1").value("ok").toBool());

    const QVariantMap retry = bookDefault();
    QCOMPARE(retry, original);
    QCOMPARE(dataOf(retry).value("expiresAtUtc").toLongLong(), StartUtc + 45 * 60);
    QCOMPARE(dataOf(retry).value("amountKopecks").toLongLong(), 13500LL);
    // а активная сессия показывает текущее состояние
    QCOMPARE(m_service->activeSession().value("expiresAtUtc").toLongLong(), StartUtc + 60 * 60);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 2);
}

void TstBookingService::bookRollsBackOnStorageError()
{
    // Тестовый триггер ломает запись оплаты: сессия не должна остаться.
    QVERIFY(execOrFail(m_db, "CREATE TEMP TRIGGER fail_payment BEFORE INSERT ON payment_operations "
                             "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    QSignalSpy spy(m_service, SIGNAL(activeSessionChanged()));
    const QVariantMap r = bookDefault();
    QVERIFY(!r.value("ok").toBool());
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("STORAGE_ERROR"));
    QVERIFY(!r.value("message").toString().contains(QStringLiteral("test failure")));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 0);
    QVERIFY(m_service->activeSession().isEmpty());
    QCOMPARE(spy.count(), 0);

    // после устранения сбоя тот же ключ проходит обычным образом
    QVERIFY(execOrFail(m_db, "DROP TRIGGER fail_payment"));
    QVERIFY(bookDefault().value("ok").toBool());
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 1);
}

void TstBookingService::sqlInjectionIsInert()
{
    const QString evil = QStringLiteral("s1' OR '1'='1");
    QCOMPARE(m_service->book("z-paid", evil, "v1", 45, "op-x").value("errorCode").toString(),
             QStringLiteral("NOT_FOUND"));
    QCOMPARE(m_service->book("z-paid", "s1", "v2' OR user_id='u1", 45, "op-y")
                 .value("errorCode").toString(),
             QStringLiteral("NOT_FOUND"));
    QCOMPARE(m_service->quote("z-paid'; DROP TABLE sessions; --", 15).value("errorCode").toString(),
             QStringLiteral("NOT_FOUND"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);

    const QVariantMap r = bookDefault(QStringLiteral("op'); DELETE FROM users; --"));
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM users"), 2);
}

void TstBookingService::extendAddsToStoredExpiryWithStoredRate()
{
    const QVariantMap booked = bookDefault();
    const QString sessionId = dataOf(booked).value("id").toString();
    const qint64 originalExpiry = StartUtc + 45 * 60;

    // тариф каталога поменялся — продление по сохранённому
    QVERIFY(execOrFail(m_db, "UPDATE zones SET rate_kopecks_per_hour=99999 WHERE id='z-paid'"));
    m_now += 30 * 60; // через полчаса, сессия ещё активна

    QSignalSpy spy(m_service, SIGNAL(activeSessionChanged()));
    const QVariantMap r = m_service->extend(sessionId, 15, "op-ext-1");
    QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("message").toString()));
    const QVariantMap d = dataOf(r);
    QCOMPARE(d.value("id").toString(), sessionId);
    QCOMPARE(d.value("expiresAtUtc").toLongLong(), originalExpiry + 15 * 60);
    QCOMPARE(d.value("chargedKopecks").toLongLong(), 4500LL);
    QCOMPARE(d.value("amountKopecks").toLongLong(), 18000LL);
    QCOMPARE(d.value("startsAtUtc").toLongLong(), StartUtc);
    QCOMPARE(d.value("ticketNumber").toString(), "DEMO-" + sessionId);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_service->activeSession().value("expiresAtUtc").toLongLong(),
             originalExpiry + 15 * 60);

    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations "
                         "WHERE kind='extend' AND amount_kopecks=4500"), 1);
}

void TstBookingService::extendIsIdempotent()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();
    const QVariantMap first = m_service->extend(sessionId, 15, "op-ext-1");
    QVERIFY(first.value("ok").toBool());
    m_now += 3;
    QCOMPARE(m_service->extend(sessionId, 15, "op-ext-1"), first);
    QCOMPARE(m_service->activeSession().value("expiresAtUtc").toLongLong(),
             StartUtc + 60 * 60);

    // новый ключ — второе продление от сохранённого expiresAtUtc
    const QVariantMap second = m_service->extend(sessionId, 30, "op-ext-2");
    QVERIFY(second.value("ok").toBool());
    QCOMPARE(dataOf(second).value("expiresAtUtc").toLongLong(), StartUtc + 90 * 60);
    QCOMPARE(dataOf(second).value("amountKopecks").toLongLong(), 13500LL + 4500 + 9000);
    // повтор первого ключа после второго продления — исходный ответ
    QCOMPARE(m_service->extend(sessionId, 15, "op-ext-1"), first);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations WHERE kind='extend'"), 2);
    QCOMPARE(count(m_db, "SELECT amount_kopecks FROM sessions"), 27000);
}

void TstBookingService::extendRejectsForeignUnknownAndExpired()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();

    QCOMPARE(m_service->extend("нет-сессии", 15, "op-e1").value("errorCode").toString(),
             QStringLiteral("NOT_FOUND"));

    m_context->setUserId(QStringLiteral("u2"));
    const QVariantMap foreign = m_service->extend(sessionId, 15, "op-e2");
    QCOMPARE(foreign.value("errorCode").toString(), QStringLiteral("NOT_OWNER"));
    QVERIFY(dataOf(foreign).isEmpty());

    m_context->setUserId(QStringLiteral("u1"));
    m_now = StartUtc + 45 * 60; // ровно момент окончания — уже истекла
    QCOMPARE(m_service->extend(sessionId, 15, "op-e3").value("errorCode").toString(),
             QStringLiteral("SESSION_EXPIRED"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='completed'"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations WHERE kind='extend'"), 0);
    QVERIFY(m_service->activeSession().isEmpty());
}

void TstBookingService::extendReusedKeyConflicts()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();
    QVERIFY(m_service->extend(sessionId, 15, "op-ext-1").value("ok").toBool());

    QCOMPARE(m_service->extend(sessionId, 30, "op-ext-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    // ключ бронирования нельзя использовать для продления и наоборот
    QCOMPARE(m_service->extend(sessionId, 15, "op-book-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QCOMPARE(m_service->book("z-paid", "s1", "v1", 15, "op-ext-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 2);
    QCOMPARE(m_service->activeSession().value("expiresAtUtc").toLongLong(), StartUtc + 60 * 60);
}

void TstBookingService::amountAboveLimitIsRejected()
{
    QVERIFY(execOrFail(m_db, "INSERT INTO zones VALUES ('z-huge','Огромная','Адрес 4',"
                             "55.78,37.64,36028797018963972,'Круглосуточно')"));
    QVERIFY(execOrFail(m_db, "INSERT INTO spaces VALUES ('h1','z-huge','01',0)"));

    QCOMPARE(m_service->quote("z-huge", 15).value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    const QVariantMap r = m_service->book("z-huge", "h1", "v1", 15, "op-huge");
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("INVALID_INPUT"));
    QVERIFY(dataOf(r).isEmpty());
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 0);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 0);
}

void TstBookingService::largeAmountSurvivesReplayExactly()
{
    // 375299968947541 коп./час × 24 ч = 9007199254740984 ≤ 2^53−1
    QVERIFY(execOrFail(m_db, "INSERT INTO zones VALUES ('z-big','Дорогая','Адрес 5',"
                             "55.79,37.65,375299968947541,'Круглосуточно')"));
    QVERIFY(execOrFail(m_db, "INSERT INTO spaces VALUES ('b1','z-big','01',0)"));

    const QVariantMap first = m_service->book("z-big", "b1", "v1", 1440, "op-big");
    QVERIFY2(first.value("ok").toBool(), qPrintable(first.value("message").toString()));
    QCOMPARE(dataOf(first).value("amountKopecks").toLongLong(), 9007199254740984LL);
    const QVariantMap replay = m_service->book("z-big", "b1", "v1", 1440, "op-big");
    QCOMPARE(replay, first);
    QCOMPARE(dataOf(replay).value("amountKopecks").toLongLong(), 9007199254740984LL);

    // продление вывело бы итог за предел — отказ без записи
    const QString sessionId = dataOf(first).value("id").toString();
    QCOMPARE(m_service->extend(sessionId, 15, "op-big-ext").value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);
    QCOMPARE(m_service->activeSession().value("amountKopecks").toLongLong(),
             9007199254740984LL);
}

void TstBookingService::reusedKeyWithInvalidInputConflicts()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();
    QVERIFY(m_service->extend(sessionId, 15, "op-ext-1").value("ok").toBool());

    QCOMPARE(m_service->book("z-paid", "s1", "v1", 16, "op-book-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QCOMPARE(m_service->book("", "s1", "v1", 45, "op-book-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QCOMPARE(m_service->extend(sessionId, 16, "op-ext-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    QCOMPARE(m_service->extend("", 15, "op-ext-1").value("errorCode").toString(),
             QStringLiteral("IDEMPOTENCY_CONFLICT"));
    // новый ключ с неверными данными — по-прежнему INVALID_INPUT
    QCOMPARE(m_service->book("z-paid", "s1", "v1", 16, "op-new").value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(m_service->book("z-paid", "s1", "v1", 45, "").value("errorCode").toString(),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 2);
}

void TstBookingService::extendRollsBackOnStorageError()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();
    QVERIFY(execOrFail(m_db, "CREATE TEMP TRIGGER fail_payment BEFORE INSERT ON payment_operations "
                             "BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
    QSignalSpy spy(m_service, SIGNAL(activeSessionChanged()));
    QCOMPARE(m_service->extend(sessionId, 15, "op-ext-1").value("errorCode").toString(),
             QStringLiteral("STORAGE_ERROR"));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(m_service->activeSession().value("expiresAtUtc").toLongLong(), StartUtc + 45 * 60);
    QCOMPARE(count(m_db, "SELECT amount_kopecks FROM sessions"), 13500);
    QCOMPARE(count(m_db, QString("SELECT expires_at_utc - %1 FROM sessions").arg(StartUtc)),
             45 * 60);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 1);
}

void TstBookingService::extendRetryAfterExpiryReturnsOriginalResponse()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();
    const QVariantMap first = m_service->extend(sessionId, 15, "op-ext-1");
    QVERIFY(first.value("ok").toBool());

    m_now = StartUtc + 3 * 3600; // сессия давно истекла
    QCOMPARE(m_service->extend(sessionId, 15, "op-ext-1"), first);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='completed'"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM payment_operations"), 2);
    QVERIFY(m_service->activeSession().isEmpty());
    // новым ключом истёкшую не продлить
    QCOMPARE(m_service->extend(sessionId, 15, "op-ext-2").value("errorCode").toString(),
             QStringLiteral("SESSION_EXPIRED"));
}

void TstBookingService::bookExpiredSpaceWithoutRefresh()
{
    QVERIFY(bookDefault().value("ok").toBool());
    m_now = StartUtc + 45 * 60; // истекла, refresh не вызывался
    QSqlQuery update(m_db);
    QVERIFY(update.exec("SELECT COUNT(*) FROM sessions WHERE status='active'"));
    QVERIFY(update.next());
    QCOMPARE(update.value(0).toInt(), 1);

    m_context->blockSignals(true); // смена пользователя без автоматического refresh
    m_context->setUserId(QStringLiteral("u2"));
    m_context->blockSignals(false);
    const QVariantMap r = m_service->book("z-paid", "s1", "v2", 15, "op-u2");
    QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("message").toString()));
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='active'"), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='completed'"), 1);
}

void TstBookingService::refreshCompletesExpiredAndFreesSpace()
{
    const QString sessionId = dataOf(bookDefault()).value("id").toString();

    m_now = StartUtc + 45 * 60 - 1;
    m_service->refresh();
    QCOMPARE(m_service->activeSession().value("id").toString(), sessionId);

    QSignalSpy spy(m_service, SIGNAL(activeSessionChanged()));
    m_now = StartUtc + 45 * 60 + 10;
    m_service->refresh();
    QVERIFY(m_service->activeSession().isEmpty());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='completed'"), 1);
    QVERIFY(m_service->history().isEmpty()); // история — этап 4

    // повторный refresh не оживляет сессию и не шлёт лишний сигнал
    m_service->refresh();
    QCOMPARE(spy.count(), 1);

    // место освобождено для другого пользователя, исходный ответ сохранён
    m_context->setUserId(QStringLiteral("u2"));
    QVERIFY(m_service->book("z-paid", "s1", "v2", 15, "op-u2").value("ok").toBool());
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(dataOf(bookDefault()).value("ticketNumber").toString(), "DEMO-" + sessionId);
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions"), 2);
}

void TstBookingService::restartRestoresActiveSession()
{
    const QVariantMap booked = dataOf(bookDefault());
    delete m_service;
    m_service = nullptr;

    m_now += 20 * 60; // «сон» приложения
    m_service = new TestBookingService(m_db, m_context, &m_now);
    m_service->refresh();
    const QVariantMap restored = m_service->activeSession();
    QCOMPARE(restored.value("id").toString(), booked.value("id").toString());
    QCOMPARE(restored.value("expiresAtUtc").toLongLong(), StartUtc + 45 * 60);
    QCOMPARE(restored.value("plate").toString(), QStringLiteral("А123ВС77"));
    QCOMPARE(restored.value("zoneId").toString(), QStringLiteral("z-paid"));
    QCOMPARE(restored.value("paymentIsDemo").toBool(), true);

    // рестарт после окончания — сессия не оживает
    delete m_service;
    m_now = StartUtc + 2 * 3600;
    m_service = new TestBookingService(m_db, m_context, &m_now);
    m_service->refresh();
    QVERIFY(m_service->activeSession().isEmpty());
    QCOMPARE(count(m_db, "SELECT COUNT(*) FROM sessions WHERE status='active'"), 0);
}

void TstBookingService::userChangeReloadsActiveSession()
{
    QVERIFY(bookDefault().value("ok").toBool());
    QVERIFY(!m_service->activeSession().isEmpty());
    m_context->setUserId(QStringLiteral("u2"));
    QVERIFY(m_service->activeSession().isEmpty());
    m_context->setUserId(QString());
    QVERIFY(m_service->activeSession().isEmpty());
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->activeSession().value("vehicleId").toString(), QStringLiteral("v1"));
}

QTEST_GUILESS_MAIN(TstBookingService)
#include "tst_bookingservice.moc"
