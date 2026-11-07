// Проверки HistoryService (#22) на SQLite со схемой migrations/001_initial.sql и настоящим core.
// Хостовые тесты: подтверждают SQL и правила, но не сборку в Aurora SDK.
#include <QtTest>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include "booking/bookingservice.h"
#include "core/sessioncontext.h"
#include "history/historyservice.h"

namespace {

const qint64 StartUtc = 1762538400; // 2025-11-07 18:00:00 UTC

class TestHistoryService : public HistoryService
{
public:
    TestHistoryService(QSqlDatabase db, SessionContext *context, qint64 *clock)
        : HistoryService(db, context), m_clock(clock) {}

protected:
    qint64 currentUtc() const override { return *m_clock; }

private:
    qint64 *m_clock;
};

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

QString schemaPath()
{
    const QByteArray env = qgetenv("HISTORY_SCHEMA_SQL");
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env);
    return QStringLiteral(HISTORY_SOURCE_ROOT "/migrations/001_initial.sql");
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

qint64 scalar(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return -1;
    return q.value(0).toLongLong();
}

bool applySchema(QSqlDatabase &db)
{
    if (!execOrFail(db, QStringLiteral("PRAGMA foreign_keys = ON")))
        return false;
    QFile file(schemaPath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QString sql = QString::fromUtf8(file.readAll());
    sql.remove(QRegularExpression(QStringLiteral("--[^\n]*")));
    const QStringList statements = sql.split(QLatin1Char(';'));
    for (const QString &statement : statements) {
        if (!statement.trimmed().isEmpty() && !execOrFail(db, statement))
            return false;
    }
    const QStringList fixtures = {
        "INSERT INTO users(id, contact, display_name) VALUES ('u1','u1@demo.test','Первый')",
        "INSERT INTO users(id, contact, display_name) VALUES ('u2','u2@demo.test','Второй')",
        "INSERT INTO vehicles(id, user_id, plate, brand) VALUES ('v1','u1','А123ВС77','Лада')",
        "INSERT INTO vehicles(id, user_id, plate, brand) VALUES ('v2','u2','В456ОР99','Камаз')",
        "INSERT INTO zones VALUES ('z-paid','Платная','Адрес 1',55.75,37.61,18000,'Круглосуточно')",
        "INSERT INTO zones VALUES ('z-free','Бесплатная','Адрес 2',55.76,37.62,0,'Круглосуточно')",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('s1','z-paid','01',0)",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('s2','z-paid','02',0)",
        "INSERT INTO spaces(id, zone_id, label, mock_occupied) VALUES ('s3','z-free','01',0)",
    };
    for (const QString &statement : fixtures) {
        if (!execOrFail(db, statement))
            return false;
    }
    return true;
}

bool addSession(QSqlDatabase &db, const QString &id, const QString &user, const QString &vehicle,
                const QString &space, qint64 start, int minutes, qint64 amount,
                const QString &status = QStringLiteral("completed"))
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO sessions(id, user_id, vehicle_id, space_id, starts_at_utc, "
                             "expires_at_utc, rate_kopecks_per_hour, amount_kopecks, status, plate_snapshot) "
                             "VALUES (?, ?, ?, ?, ?, ?, 18000, ?, ?, ?)"));
    q.addBindValue(id);
    q.addBindValue(user);
    q.addBindValue(vehicle);
    q.addBindValue(space);
    q.addBindValue(start);
    q.addBindValue(start + static_cast<qint64>(minutes) * 60);
    q.addBindValue(amount);
    q.addBindValue(status);
    q.addBindValue(user == QLatin1String("u1") ? QStringLiteral("А123ВС77") : QStringLiteral("В456ОР99"));
    if (!q.exec()) {
        qWarning() << "insert session failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QStringList ids(const QVariantList &entries)
{
    QStringList result;
    for (const QVariant &entry : entries)
        result << entry.toMap().value(QStringLiteral("id")).toString();
    return result;
}

} // namespace

class TstHistoryService : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void emptyHistory();
    void notAuthenticated();
    void entryFieldsAndTicket();
    void orderStartsDescThenId();
    void usersAreIsolated();
    void userSwitchClearsBeforeReload();
    void logoutLeavesNoData();
    void extensionNotDoubleCounted();
    void freeParking();
    void expiredActiveShownCompletedWithoutWrite();
    void moreThanHundredEntries();
    void moneyLimit();
    void readErrorClearsData();
    void reopenDatabaseRestoresHistory();

private:
    QSqlDatabase m_db;
    SessionContext *m_context = nullptr;
    TestHistoryService *m_service = nullptr;
    qint64 m_now = StartUtc;
};

void TstHistoryService::init()
{
    m_now = StartUtc;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QUuid::createUuid().toString());
    m_db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(m_db.open());
    QVERIFY2(applySchema(m_db), qPrintable("Нет или не применилась схема: " + schemaPath()));
    m_context = new SessionContext;
    m_service = new TestHistoryService(m_db, m_context, &m_now);
}

void TstHistoryService::cleanup()
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

void TstHistoryService::emptyHistory()
{
    m_context->setUserId(QStringLiteral("u1"));
    const QVariantMap r = m_service->refresh();
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(m_service->entries().size(), 0);
    QCOMPARE(m_service->totalKopecks(), qint64(0));
    QCOMPARE(m_service->totalSessions(), 0);
}

void TstHistoryService::notAuthenticated()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
    const QVariantMap r = m_service->refresh();
    QCOMPARE(r.value("ok").toBool(), false);
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("NOT_AUTHENTICATED"));
    QCOMPARE(m_service->entries().size(), 0);
}

void TstHistoryService::entryFieldsAndTicket()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->entries().size(), 1);
    const QVariantMap e = m_service->entries().first().toMap();
    QCOMPARE(e.value("zoneId").toString(), QStringLiteral("z-paid"));
    QCOMPARE(e.value("zoneName").toString(), QStringLiteral("Платная"));
    QCOMPARE(e.value("address").toString(), QStringLiteral("Адрес 1"));
    QCOMPARE(e.value("plate").toString(), QStringLiteral("А123ВС77"));
    QCOMPARE(e.value("startsAtUtc").toLongLong(), StartUtc - 7200);
    QCOMPARE(e.value("expiresAtUtc").toLongLong(), StartUtc - 3600);
    QCOMPARE(e.value("amountKopecks").toLongLong(), qint64(18000));
    QCOMPARE(e.value("currency").toString(), QStringLiteral("RUB"));
    QCOMPARE(e.value("status").toString(), QStringLiteral("completed"));
    QCOMPARE(e.value("ticketNumber").toString(), QStringLiteral("DEMO-a"));
    QCOMPARE(e.value("paymentIsDemo").toBool(), true);
}

void TstHistoryService::orderStartsDescThenId()
{
    QVERIFY(addSession(m_db, "b", "u1", "v1", "s1", StartUtc - 9000, 15, 4500));
    QVERIFY(addSession(m_db, "c", "u1", "v1", "s2", StartUtc - 7200, 15, 4500));
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s3", StartUtc - 7200, 15, 0));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(ids(m_service->entries()), QStringList({"a", "c", "b"}));
}

void TstHistoryService::usersAreIsolated()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
    QVERIFY(addSession(m_db, "b", "u2", "v2", "s2", StartUtc - 3600, 30, 9000));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(ids(m_service->entries()), QStringList({"a"}));
    QCOMPARE(m_service->totalKopecks(), qint64(18000));
    m_context->setUserId(QStringLiteral("u2"));
    QCOMPARE(ids(m_service->entries()), QStringList({"b"}));
    QCOMPARE(m_service->totalKopecks(), qint64(9000));
    QCOMPARE(m_service->totalSessions(), 1);
}

void TstHistoryService::userSwitchClearsBeforeReload()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->totalSessions(), 1);

    // На каждом уведомлении после смены пользователя не должно быть записей u1.
    bool leaked = false;
    connect(m_service, &HistoryService::historyChanged, this, [this, &leaked]() {
        for (const QVariant &entry : m_service->entries()) {
            if (entry.toMap().value("id").toString() == QLatin1String("a"))
                leaked = true;
        }
    });
    m_context->setUserId(QStringLiteral("u2"));
    QVERIFY(!leaked);
    QCOMPARE(m_service->entries().size(), 0);
    QCOMPARE(m_service->totalKopecks(), qint64(0));
}

void TstHistoryService::logoutLeavesNoData()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
    m_context->setUserId(QStringLiteral("u1"));
    QSignalSpy spy(m_service, SIGNAL(historyChanged()));
    m_context->setUserId(QString());
    QVERIFY(spy.count() >= 1);
    QCOMPARE(m_service->entries().size(), 0);
    QCOMPARE(m_service->totalKopecks(), qint64(0));
    QCOMPARE(m_service->totalSessions(), 0);
    QCOMPARE(m_service->refresh().value("errorCode").toString(), QStringLiteral("NOT_AUTHENTICATED"));
}

void TstHistoryService::extensionNotDoubleCounted()
{
    // Настоящие book (45 мин = 13 500) и extend (+15 мин = 4 500) из BookingService.
    TestBookingService booking(m_db, m_context, &m_now);
    m_context->setUserId(QStringLiteral("u1"));
    const QVariantMap booked = booking.book("z-paid", "s1", "v1", 45, "op-book");
    QVERIFY2(booked.value("ok").toBool(), qPrintable(booked.value("message").toString()));
    const QString sessionId = booked.value("data").toMap().value("id").toString();
    m_now += 600;
    const QVariantMap extended = booking.extend(sessionId, 15, "op-extend");
    QVERIFY2(extended.value("ok").toBool(), qPrintable(extended.value("message").toString()));
    QCOMPARE(scalar(m_db, "SELECT SUM(amount_kopecks) FROM payment_operations"), qint64(18000));

    QVERIFY(m_service->refresh().value("ok").toBool());
    QCOMPARE(m_service->totalSessions(), 1);
    QCOMPARE(m_service->totalKopecks(), qint64(18000));
    QCOMPARE(m_service->entries().first().toMap().value("amountKopecks").toLongLong(), qint64(18000));
    QCOMPARE(m_service->entries().first().toMap().value("status").toString(), QStringLiteral("active"));
}

void TstHistoryService::freeParking()
{
    QVERIFY(addSession(m_db, "f", "u1", "v1", "s3", StartUtc - 7200, 60, 0));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->totalSessions(), 1);
    QCOMPARE(m_service->totalKopecks(), qint64(0));
    QCOMPARE(m_service->entries().first().toMap().value("amountKopecks").toLongLong(), qint64(0));
}

void TstHistoryService::expiredActiveShownCompletedWithoutWrite()
{
    QVERIFY(addSession(m_db, "x", "u1", "v1", "s1", StartUtc - 3600, 30, 9000, "active"));
    const qint64 changesBefore = scalar(m_db, "SELECT total_changes()");
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->entries().first().toMap().value("status").toString(), QStringLiteral("completed"));
    QCOMPARE(scalar(m_db, "SELECT COUNT(*) FROM sessions WHERE id = 'x' AND status = 'active'"), qint64(1));
    QCOMPARE(scalar(m_db, "SELECT total_changes()"), changesBefore);
}

void TstHistoryService::moreThanHundredEntries()
{
    for (int i = 0; i < 130; ++i) {
        QVERIFY(addSession(m_db, QString::asprintf("s%03d", i), "u1", "v1", (i % 2) ? "s1" : "s2",
                           StartUtc - 100000 + i * 600, 15, 4500));
    }
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->entries().size(), HistoryService::MaxEntries);
    QCOMPARE(m_service->totalSessions(), 130);
    QCOMPARE(m_service->totalKopecks(), qint64(130) * 4500);
    QCOMPARE(ids(m_service->entries()).first(), QStringLiteral("s129"));
}

void TstHistoryService::moneyLimit()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 15, HistoryService::MaxTotalKopecks - 1));
    QVERIFY(addSession(m_db, "b", "u1", "v1", "s2", StartUtc - 3600, 15, 1));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->totalKopecks(), HistoryService::MaxTotalKopecks);

    QVERIFY(addSession(m_db, "c", "u1", "v1", "s3", StartUtc - 1800, 15, 1));
    const QVariantMap r = m_service->refresh();
    QCOMPARE(r.value("ok").toBool(), false);
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("STORAGE_ERROR"));
    // Ошибка не выдаётся за успешный ноль: данные очищены, результат — ошибка.
    QCOMPARE(m_service->entries().size(), 0);
    QCOMPARE(m_service->totalSessions(), 0);
}

void TstHistoryService::readErrorClearsData()
{
    QVERIFY(addSession(m_db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
    m_context->setUserId(QStringLiteral("u1"));
    QCOMPARE(m_service->totalSessions(), 1);
    m_db.close();
    const QVariantMap r = m_service->refresh();
    QCOMPARE(r.value("ok").toBool(), false);
    QCOMPARE(r.value("errorCode").toString(), QStringLiteral("STORAGE_ERROR"));
    QCOMPARE(m_service->entries().size(), 0);
    QCOMPARE(m_service->totalKopecks(), qint64(0));
}

void TstHistoryService::reopenDatabaseRestoresHistory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.sqlite"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("hist-write"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QVERIFY(applySchema(db));
        QVERIFY(addSession(db, "a", "u1", "v1", "s1", StartUtc - 7200, 60, 18000));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("hist-write"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("hist-read"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        SessionContext context;
        TestHistoryService service(db, &context, &m_now);
        context.setUserId(QStringLiteral("u1"));
        QCOMPARE(service.totalSessions(), 1);
        QCOMPARE(service.totalKopecks(), qint64(18000));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("hist-read"));
}

QTEST_MAIN(TstHistoryService)
#include "tst_historyservice.moc"
