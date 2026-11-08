// SPDX-License-Identifier: MIT
//
// Проверки модуля профиля: нормализация ввода и AccountService на реальной SQLite
// в памяти. Схема ниже повторяет таблицы users/vehicles/settings из
// migrations/001_initial.sql (PR #6) в объёме, нужном модулю.
// Эти проверки не доказывают сборку в Aurora SDK.

#include "accountservice.h"
#include "accountvalidation.h"
#include "sessioncontext.h"

#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

namespace {

bool exec(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qWarning() << sql << query.lastError().text();
        return false;
    }
    return true;
}

int count(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql) || !query.next())
        return -1;
    return query.value(0).toInt();
}

QString errorCode(const QVariantMap &result)
{
    return result.value(QStringLiteral("errorCode")).toString();
}

bool ok(const QVariantMap &result)
{
    return result.value(QStringLiteral("ok")).toBool();
}

} // namespace

class TestAccount : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void contactNormalization_data();
    void contactNormalization();
    void plateAndBrand();
    void displayName();

    void wrongCodeDoesNotSignIn();
    void signInCreatesUserAndSetsSession();
    void sameContactReusesUser();
    void writeCommandsRequireSignIn();
    void resultShape();
    void addVehicleAndRejectDuplicate();
    void sqlCharactersStoredLiterally();
    void userIsolation();
    void restoreAfterRestart();
    void signOutClearsVisibleDataOnly();
    void danglingActiveUserIsSignedOut();
    void storageErrorIsReported();
    void signOutFailureKeepsSignedIn();

private:
    QSqlDatabase m_db;
    QString m_connectionName;
};

void TestAccount::init()
{
    static int counter = 0;
    m_connectionName = QStringLiteral("account-test-%1").arg(++counter);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(m_db.open());
    QVERIFY(exec(m_db, QStringLiteral("PRAGMA foreign_keys = ON")));
    QVERIFY(exec(m_db, QStringLiteral(
            "CREATE TABLE users ("
            " id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),"
            " contact TEXT NOT NULL UNIQUE CHECK (length(contact) BETWEEN 1 AND 254),"
            " display_name TEXT NOT NULL DEFAULT '')")));
    QVERIFY(exec(m_db, QStringLiteral(
            "CREATE TABLE vehicles ("
            " id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),"
            " user_id TEXT NOT NULL REFERENCES users(id),"
            " plate TEXT NOT NULL CHECK (length(plate) BETWEEN 4 AND 15),"
            " brand TEXT NOT NULL CHECK (length(brand) BETWEEN 1 AND 60),"
            " UNIQUE (user_id, plate), UNIQUE (id, user_id))")));
    QVERIFY(exec(m_db, QStringLiteral(
            "CREATE TABLE settings (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)")));
}

void TestAccount::cleanup()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

void TestAccount::contactNormalization_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("expected");

    QTest::newRow("email trim lower") << "  User.Name@Example.COM " << true << "user.name@example.com";
    QTest::newRow("email two @") << "a@b@c.ru" << false << "";
    QTest::newRow("email empty local") << "@example.ru" << false << "";
    QTest::newRow("email empty domain") << "name@" << false << "";
    QTest::newRow("email inner space") << "na me@example.ru" << false << "";
    // "@example.ru" — 11 символов.
    QTest::newRow("email 254") << QString(243, QLatin1Char('a')) + "@example.ru" << true
                               << QString(243, QLatin1Char('a')) + "@example.ru";
    QTest::newRow("email 255") << QString(244, QLatin1Char('a')) + "@example.ru" << false << "";
    QTest::newRow("phone plus formatted") << "+7 (999) 123-45-67" << true << "+79991234567";
    QTest::newRow("phone no plus") << "8 999 123 45 67" << true << "89991234567";
    QTest::newRow("phone 15 digits") << "123456789012345" << true << "123456789012345";
    QTest::newRow("phone 9 digits") << "123456789" << false << "";
    QTest::newRow("phone 16 digits") << "1234567890123456" << false << "";
    QTest::newRow("phone letters") << "+7999abc4567" << false << "";
    QTest::newRow("phone plus in middle") << "7+9991234567" << false << "";
    QTest::newRow("phone dots") << "999.123.45.67" << false << "";
    QTest::newRow("empty") << "   " << false << "";
}

void TestAccount::contactNormalization()
{
    QFETCH(QString, input);
    QFETCH(bool, valid);
    QFETCH(QString, expected);

    QString normalized;
    QCOMPARE(AccountValidation::normalizeContact(input, &normalized), valid);
    if (valid)
        QCOMPARE(normalized, expected);
}

void TestAccount::plateAndBrand()
{
    QString value;
    QVERIFY(AccountValidation::normalizePlate(QStringLiteral("  а123вс77 "), &value));
    QCOMPARE(value, QStringLiteral("А123ВС77"));
    QVERIFY(AccountValidation::normalizePlate(QStringLiteral("ab12"), &value));
    QCOMPARE(value, QStringLiteral("AB12"));
    QVERIFY(!AccountValidation::normalizePlate(QStringLiteral(" ab1 "), &value));
    QVERIFY(!AccountValidation::normalizePlate(QString(16, QLatin1Char('A')), &value));
    QVERIFY(AccountValidation::normalizePlate(QString(15, QLatin1Char('A')), &value));

    QVERIFY(AccountValidation::normalizeBrand(QStringLiteral("  Lada Vesta "), &value));
    QCOMPARE(value, QStringLiteral("Lada Vesta"));
    QVERIFY(!AccountValidation::normalizeBrand(QStringLiteral("   "), &value));
    QVERIFY(AccountValidation::normalizeBrand(QString(60, QLatin1Char('x')), &value));
    QVERIFY(!AccountValidation::normalizeBrand(QString(61, QLatin1Char('x')), &value));
}

void TestAccount::displayName()
{
    QString value;
    QVERIFY(AccountValidation::normalizeDisplayName(QStringLiteral(" Матвей "), &value));
    QCOMPARE(value, QStringLiteral("Матвей"));
    QVERIFY(!AccountValidation::normalizeDisplayName(QStringLiteral("  "), &value));
    QVERIFY(!AccountValidation::normalizeDisplayName(QString(101, QLatin1Char('x')), &value));
}

void TestAccount::wrongCodeDoesNotSignIn()
{
    SessionContext session;
    AccountService service(m_db, &session);

    QVariantMap result = service.demoSignIn(QStringLiteral("user@example.ru"),
                                            QStringLiteral("000000"));
    QVERIFY(!ok(result));
    QCOMPARE(errorCode(result), QStringLiteral("INVALID_INPUT"));
    QVERIFY(!service.signedIn());
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM users")), 0);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM settings")), 0);

    result = service.demoSignIn(QStringLiteral("not-a-contact"), QStringLiteral("123456"));
    QCOMPARE(errorCode(result), QStringLiteral("INVALID_INPUT"));
    QVERIFY(session.userId().isEmpty());
}

void TestAccount::signInCreatesUserAndSetsSession()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QSignalSpy signedInSpy(&service, SIGNAL(signedInChanged()));
    QSignalSpy profileSpy(&service, SIGNAL(profileChanged()));
    QSignalSpy sessionSpy(&session, SIGNAL(userChanged()));

    const QVariantMap result = service.demoSignIn(QStringLiteral(" User@Example.ru "),
                                                  QStringLiteral(" 123456 "));
    QVERIFY2(ok(result), qPrintable(result.value(QStringLiteral("message")).toString()));
    QVERIFY(service.signedIn());
    QCOMPARE(signedInSpy.count(), 1);
    QCOMPARE(profileSpy.count(), 1);
    QCOMPARE(sessionSpy.count(), 1);

    const QVariantMap profile = service.profile();
    QCOMPARE(profile.value(QStringLiteral("contact")).toString(),
             QStringLiteral("user@example.ru"));
    QCOMPARE(profile.value(QStringLiteral("displayName")).toString(), QString());
    const QString id = profile.value(QStringLiteral("id")).toString();
    QCOMPARE(id.size(), 36);
    QCOMPARE(session.userId(), id);
    QCOMPARE(result.value(QStringLiteral("data")).toMap()
                     .value(QStringLiteral("profile")).toMap(), profile);

    QSqlQuery query(m_db);
    QVERIFY(query.exec(QStringLiteral("SELECT value FROM settings WHERE key = 'active_user_id'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), id);
}

void TestAccount::sameContactReusesUser()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(ok(service.demoSignIn(QStringLiteral("+7 999 123-45-67"), QStringLiteral("123456"))));
    const QString firstId = service.profile().value(QStringLiteral("id")).toString();
    service.signOut();

    QVERIFY(ok(service.demoSignIn(QStringLiteral("+7(999)1234567"), QStringLiteral("123456"))));
    QCOMPARE(service.profile().value(QStringLiteral("id")).toString(), firstId);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM users")), 1);
}

void TestAccount::writeCommandsRequireSignIn()
{
    SessionContext session;
    AccountService service(m_db, &session);

    QCOMPARE(errorCode(service.saveProfile(QStringLiteral("Имя"))),
             QStringLiteral("NOT_AUTHENTICATED"));
    QCOMPARE(errorCode(service.addVehicle(QStringLiteral("A123BC77"), QStringLiteral("Lada"))),
             QStringLiteral("NOT_AUTHENTICATED"));
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM vehicles")), 0);
}

void TestAccount::resultShape()
{
    SessionContext session;
    AccountService service(m_db, &session);

    const QVariantMap failure = service.saveProfile(QStringLiteral("x"));
    QVERIFY(failure.contains(QStringLiteral("ok")));
    QVERIFY(!failure.value(QStringLiteral("message")).toString().isEmpty());
    QVERIFY(failure.value(QStringLiteral("data")).toMap().isEmpty());

    const QVariantMap success = service.demoSignIn(QStringLiteral("a@b.ru"),
                                                   QStringLiteral("123456"));
    QVERIFY(ok(success));
    QCOMPARE(errorCode(success), QString());
    QCOMPARE(success.value(QStringLiteral("message")).toString(), QString());
}

void TestAccount::addVehicleAndRejectDuplicate()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(ok(service.demoSignIn(QStringLiteral("a@b.ru"), QStringLiteral("123456"))));
    QSignalSpy vehiclesSpy(&service, SIGNAL(vehiclesChanged()));

    QVariantMap result = service.addVehicle(QStringLiteral(" а123вс77 "),
                                            QStringLiteral(" Lada Vesta "));
    QVERIFY(ok(result));
    QCOMPARE(vehiclesSpy.count(), 1);
    const QVariantMap vehicle = result.value(QStringLiteral("data")).toMap()
                                        .value(QStringLiteral("vehicle")).toMap();
    QCOMPARE(vehicle.value(QStringLiteral("plate")).toString(), QStringLiteral("А123ВС77"));
    QCOMPARE(vehicle.value(QStringLiteral("brand")).toString(), QStringLiteral("Lada Vesta"));
    QCOMPARE(service.vehicles().size(), 1);
    QCOMPARE(service.vehicles().first().toMap(), vehicle);

    result = service.addVehicle(QStringLiteral("А123ВС77"), QStringLiteral("Другая"));
    QVERIFY(!ok(result));
    QCOMPARE(errorCode(result), QStringLiteral("INVALID_INPUT"));
    QCOMPARE(service.vehicles().size(), 1);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM vehicles")), 1);

    QCOMPARE(errorCode(service.addVehicle(QStringLiteral("ab1"), QStringLiteral("Lada"))),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(errorCode(service.addVehicle(QStringLiteral("B456CD99"), QString())),
             QStringLiteral("INVALID_INPUT"));
    QCOMPARE(vehiclesSpy.count(), 1);
}

void TestAccount::sqlCharactersStoredLiterally()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(ok(service.demoSignIn(QStringLiteral("o'brien@example.ru"), QStringLiteral("123456"))));

    const QString evilName = QStringLiteral("Robert'); DROP TABLE users;--");
    QVERIFY(ok(service.saveProfile(evilName)));
    const QString evilBrand = QStringLiteral("x' OR '1'='1");
    QVERIFY(ok(service.addVehicle(QStringLiteral("'; --"), evilBrand)));

    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM users")), 1);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM vehicles")), 1);
    QCOMPARE(service.profile().value(QStringLiteral("displayName")).toString(), evilName);
    QCOMPARE(service.profile().value(QStringLiteral("contact")).toString(),
             QStringLiteral("o'brien@example.ru"));

    AccountService reloaded(m_db, nullptr);
    QCOMPARE(reloaded.profile().value(QStringLiteral("displayName")).toString(), evilName);
    QCOMPARE(reloaded.vehicles().first().toMap().value(QStringLiteral("brand")).toString(),
             evilBrand);
}

void TestAccount::userIsolation()
{
    SessionContext session;
    AccountService service(m_db, &session);

    QVERIFY(ok(service.demoSignIn(QStringLiteral("alice@example.ru"), QStringLiteral("123456"))));
    const QString aliceId = session.userId();
    QVERIFY(ok(service.addVehicle(QStringLiteral("A111AA77"), QStringLiteral("Lada"))));
    QVERIFY(ok(service.saveProfile(QStringLiteral("Алиса"))));
    service.signOut();

    QVERIFY(ok(service.demoSignIn(QStringLiteral("bob@example.ru"), QStringLiteral("123456"))));
    QVERIFY(session.userId() != aliceId);
    QVERIFY(service.vehicles().isEmpty());
    QCOMPARE(service.profile().value(QStringLiteral("displayName")).toString(), QString());
    // Тот же номер у другого пользователя допустим.
    QVERIFY(ok(service.addVehicle(QStringLiteral("A111AA77"), QStringLiteral("Kia"))));
    QCOMPARE(service.vehicles().size(), 1);
    QCOMPARE(service.vehicles().first().toMap().value(QStringLiteral("brand")).toString(),
             QStringLiteral("Kia"));

    // Смена пользователя без явного выхода тоже переключает данные.
    QVERIFY(ok(service.demoSignIn(QStringLiteral("alice@example.ru"), QStringLiteral("123456"))));
    QCOMPARE(session.userId(), aliceId);
    QCOMPARE(service.vehicles().size(), 1);
    QCOMPARE(service.vehicles().first().toMap().value(QStringLiteral("brand")).toString(),
             QStringLiteral("Lada"));
    QCOMPARE(service.profile().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Алиса"));
}

void TestAccount::restoreAfterRestart()
{
    QString userId;
    {
        SessionContext session;
        AccountService service(m_db, &session);
        QVERIFY(ok(service.demoSignIn(QStringLiteral("a@b.ru"), QStringLiteral("123456"))));
        QVERIFY(ok(service.saveProfile(QStringLiteral("Матвей"))));
        QVERIFY(ok(service.addVehicle(QStringLiteral("A123BC77"), QStringLiteral("Lada"))));
        userId = session.userId();
    }

    SessionContext session;
    AccountService restored(m_db, &session);
    QVERIFY(restored.signedIn());
    QCOMPARE(session.userId(), userId);
    QCOMPARE(restored.profile().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Матвей"));
    QCOMPARE(restored.vehicles().size(), 1);
    QCOMPARE(restored.vehicles().first().toMap().value(QStringLiteral("plate")).toString(),
             QStringLiteral("A123BC77"));
}

void TestAccount::signOutClearsVisibleDataOnly()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(ok(service.demoSignIn(QStringLiteral("a@b.ru"), QStringLiteral("123456"))));
    QVERIFY(ok(service.addVehicle(QStringLiteral("A123BC77"), QStringLiteral("Lada"))));
    QSignalSpy signedInSpy(&service, SIGNAL(signedInChanged()));
    QSignalSpy vehiclesSpy(&service, SIGNAL(vehiclesChanged()));

    service.signOut();
    QVERIFY(!service.signedIn());
    QVERIFY(service.profile().isEmpty());
    QVERIFY(service.vehicles().isEmpty());
    QVERIFY(session.userId().isEmpty());
    QCOMPARE(signedInSpy.count(), 1);
    QCOMPARE(vehiclesSpy.count(), 1);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM settings")), 0);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM users")), 1);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM vehicles")), 1);

    AccountService afterRestart(m_db, nullptr);
    QVERIFY(!afterRestart.signedIn());
}

void TestAccount::danglingActiveUserIsSignedOut()
{
    QVERIFY(exec(m_db, QStringLiteral(
            "INSERT INTO settings (key, value) VALUES ('active_user_id', 'missing')")));
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(!service.signedIn());
    QVERIFY(session.userId().isEmpty());
}

void TestAccount::storageErrorIsReported()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(ok(service.demoSignIn(QStringLiteral("a@b.ru"), QStringLiteral("123456"))));

    QVERIFY(exec(m_db, QStringLiteral("DROP TABLE vehicles")));
    const QVariantMap result = service.addVehicle(QStringLiteral("A123BC77"),
                                                  QStringLiteral("Lada"));
    QVERIFY(!ok(result));
    QCOMPARE(errorCode(result), QStringLiteral("STORAGE_ERROR"));
    QVERIFY(service.vehicles().isEmpty());

    // Ошибка внутри транзакции входа не оставляет нового пользователя.
    service.signOut();
    QVERIFY(exec(m_db, QStringLiteral("DROP TABLE settings")));
    const QVariantMap signIn = service.demoSignIn(QStringLiteral("new@b.ru"),
                                                  QStringLiteral("123456"));
    QCOMPARE(errorCode(signIn), QStringLiteral("STORAGE_ERROR"));
    QVERIFY(!service.signedIn());
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM users WHERE contact = 'new@b.ru'")),
             0);
}

// Регрессия ревью PR16: при ошибке удаления settings.active_user_id выход не должен
// объявляться успешным, иначе после перезапуска вход «возвращается».
void TestAccount::signOutFailureKeepsSignedIn()
{
    SessionContext session;
    AccountService service(m_db, &session);
    QVERIFY(ok(service.demoSignIn(QStringLiteral("a@b.ru"), QStringLiteral("123456"))));
    QVERIFY(ok(service.addVehicle(QStringLiteral("A123BC77"), QStringLiteral("Lada"))));
    const QString userId = session.userId();
    QVERIFY(!userId.isEmpty());

    QSignalSpy signedInSpy(&service, SIGNAL(signedInChanged()));
    QSignalSpy failedSpy(&service, SIGNAL(operationFailed(QString,QString)));

    // Запись в БД запрещена: DELETE завершается ошибкой.
    QVERIFY(exec(m_db, QStringLiteral("PRAGMA query_only = ON")));
    service.signOut();

    QVERIFY(service.signedIn());
    QCOMPARE(session.userId(), userId);
    QVERIFY(!service.profile().isEmpty());
    QCOMPARE(service.vehicles().size(), 1);
    QCOMPARE(signedInSpy.count(), 0);
    QVERIFY(failedSpy.isValid());
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.first().at(0).toString(), QStringLiteral("STORAGE_ERROR"));
    QVERIFY(!failedSpy.first().at(1).toString().isEmpty());
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM settings")), 1);
    {
        // Состояние интерфейса совпадает с тем, что восстановится после перезапуска.
        AccountService afterRestart(m_db, nullptr);
        QVERIFY(afterRestart.signedIn());
    }

    // После снятия запрета повторный выход проходит и не возвращается после перезапуска.
    QVERIFY(exec(m_db, QStringLiteral("PRAGMA query_only = OFF")));
    service.signOut();
    QVERIFY(!service.signedIn());
    QVERIFY(session.userId().isEmpty());
    QCOMPARE(signedInSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(count(m_db, QStringLiteral("SELECT count(*) FROM settings")), 0);

    AccountService afterSignOut(m_db, nullptr);
    QVERIFY(!afterSignOut.signedIn());
}

QTEST_GUILESS_MAIN(TestAccount)
#include "tst_account.moc"
