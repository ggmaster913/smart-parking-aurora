#include <QtTest>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "core/databasemanager.h"
#include "core/sessioncontext.h"

class CoreTest : public QObject {
    Q_OBJECT
private slots:
    void migrationAndForeignKeys() {
        QTemporaryDir dir;
        DatabaseManager db(dir.filePath("parking.sqlite"));
        QVERIFY2(db.open(), qPrintable(db.lastError()));
        QSqlQuery q(db.connection());
        QVERIFY(q.exec("PRAGMA user_version")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 1);
        QVERIFY(q.exec("PRAGMA foreign_keys")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 1);
        QVERIFY(!q.exec("INSERT INTO vehicles VALUES('v','missing','А123ВС77','Демо')"));
        QVERIFY(db.open());
    }
    void reopeningPreservesData() {
        QTemporaryDir dir;
        const QString path = dir.filePath("parking.sqlite");
        {
            DatabaseManager db(path); QVERIFY(db.open());
            QSqlQuery q(db.connection());
            QVERIFY(q.exec("INSERT INTO users VALUES('u','demo@example.test','Профиль')"));
        }
        DatabaseManager reopened(path); QVERIFY(reopened.open());
        QSqlQuery q(reopened.connection());
        QVERIFY(q.exec("SELECT display_name FROM users WHERE id='u'"));
        QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), QString::fromUtf8("Профиль"));
    }
    void migrationFailureRollsBack() {
        QTemporaryDir dir;
        const QString sqlPath = dir.filePath("broken.sql");
        QFile file(sqlPath); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("BEGIN IMMEDIATE; CREATE TABLE should_not_exist(id INTEGER); INSERT INTO absent VALUES(1); COMMIT;");
        file.close();
        DatabaseManager db(dir.filePath("parking.sqlite"), sqlPath);
        QVERIFY(!db.open()); QVERIFY(!db.lastError().isEmpty());
        QVERIFY(!db.connection().isOpen());
        DatabaseManager retry(dir.filePath("parking.sqlite")); QVERIFY(retry.open());
        QSqlQuery q(retry.connection());
        QVERIFY(q.exec("SELECT count(*) FROM sqlite_master WHERE name='should_not_exist'"));
        QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 0);
    }
    void missingMigrationVersionRollsBack() {
        QTemporaryDir dir;
        QFile file(dir.filePath("incomplete.sql")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("BEGIN IMMEDIATE; CREATE TABLE incomplete_migration(id INTEGER); COMMIT;"); file.close();
        DatabaseManager broken(dir.filePath("parking.sqlite"), file.fileName()); QVERIFY(!broken.open());
        DatabaseManager retry(dir.filePath("parking.sqlite")); QVERIFY(retry.open());
        QSqlQuery q(retry.connection());
        QVERIFY(q.exec("SELECT count(*) FROM sqlite_master WHERE name='incomplete_migration'"));
        QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 0);
    }
    void futureSchemaIsNotDowngraded() {
        QTemporaryDir dir; const QString path = dir.filePath("parking.sqlite");
        {
            DatabaseManager db(path); QVERIFY(db.open());
            QSqlQuery q(db.connection()); QVERIFY(q.exec("PRAGMA user_version=2"));
        }
        DatabaseManager db(path); QVERIFY(!db.open()); QVERIFY(!db.lastError().isEmpty());
    }
    void distinctConnectionsDoNotShareData() {
        QTemporaryDir dir;
        DatabaseManager first(dir.filePath("one.sqlite")), second(dir.filePath("two.sqlite"));
        QVERIFY(first.open()); QVERIFY(second.open());
        QVERIFY(first.connection().connectionName() != second.connection().connectionName());
        QSqlQuery a(first.connection()); QVERIFY(a.exec("INSERT INTO users VALUES('u','one@example.test','')"));
        QSqlQuery b(second.connection()); QVERIFY(b.exec("SELECT count(*) FROM users"));
        QVERIFY(b.next()); QCOMPARE(b.value(0).toInt(), 0);
    }
    void sessionNotifiesOnlyOnUserChange() {
        SessionContext context;
        QSignalSpy spy(&context, SIGNAL(userChanged()));
        QCOMPARE(context.userId(), QString());
        context.setUserId("u"); QCOMPARE(context.userId(), QString("u")); QCOMPARE(spy.count(), 1);
        context.setUserId("u"); QCOMPARE(spy.count(), 1);
        context.setUserId(QString()); QCOMPARE(context.userId(), QString()); QCOMPARE(spy.count(), 2);
    }
};
QTEST_GUILESS_MAIN(CoreTest)
#include "tst_core.moc"
