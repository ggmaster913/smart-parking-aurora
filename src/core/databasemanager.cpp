#include "databasemanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>
#include <QVariant>

DatabaseManager::DatabaseManager(const QString &databasePath, const QString &migrationPath)
    : m_path(databasePath), m_migrationPath(migrationPath),
      m_connectionName(QStringLiteral("smart-parking-") + QUuid::createUuid().toString()) {}

DatabaseManager::~DatabaseManager() {
    // Сервисы и запросы должны быть уничтожены раньше менеджера.
    if (m_database.isValid()) m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::fail(const QString &message) {
    m_error = message;
    m_database.close();
    return false;
}

bool DatabaseManager::open() {
    if (m_database.isOpen()) return true;
    m_error.clear();
    if (m_path.isEmpty()) {
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (directory.isEmpty()) {
            m_error = QString::fromUtf8("Каталог данных приложения недоступен");
            return false;
        }
        m_path = QDir(directory).filePath(QStringLiteral("parking.sqlite"));
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        m_error = QString::fromUtf8("Не удалось создать каталог базы данных");
        return false;
    }
    if (!m_database.isValid())
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_path);
    if (!m_database.open()) return fail(m_database.lastError().text());
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys=ON"))) return fail(query.lastError().text());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return fail(QString::fromUtf8("Не удалось прочитать версию базы данных"));
    const int version = query.value(0).toInt();
    query.finish();
    if (version == 1) return true;
    if (version != 0) return fail(QString::fromUtf8("Неподдерживаемая версия базы данных"));

    QFile migration(m_migrationPath.isEmpty()
                    ? QStringLiteral(":/migrations/001_initial.sql") : m_migrationPath);
    if (!migration.open(QIODevice::ReadOnly)) return fail(migration.errorString());
    QString sql = QString::fromUtf8(migration.readAll());
    // Контролируемая начальная миграция не содержит триггеров и ';' в литералах.
    // Менеджер удерживает транзакцию до проверки версии, даже если файл содержит COMMIT.
    QStringList content;
    for (const QString &line : sql.split(QLatin1Char('\n')))
        if (!line.trimmed().startsWith(QStringLiteral("--"))) content.append(line);
    sql = content.join(QLatin1Char('\n'));
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) return fail(query.lastError().text());
    const QStringList statements = sql.split(QLatin1Char(';'));
    for (const QString &statement : statements) {
        const QString normalized = statement.trimmed().toUpper();
        if (normalized.isEmpty() || normalized == QStringLiteral("BEGIN IMMEDIATE")
                || normalized == QStringLiteral("BEGIN") || normalized == QStringLiteral("COMMIT")) continue;
        if (!query.exec(statement)) {
            const QString error = query.lastError().text();
            query.finish();
            m_database.rollback();
            return fail(error);
        }
        query.finish();
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next() || query.value(0).toInt() != 1) {
        query.finish();
        m_database.rollback();
        return fail(QString::fromUtf8("Миграция не установила ожидаемую версию базы данных"));
    }
    query.finish();
    if (!m_database.commit()) {
        const QString error = m_database.lastError().text();
        m_database.rollback();
        return fail(error);
    }
    return true;
}

QSqlDatabase DatabaseManager::connection() const { return m_database; }
QString DatabaseManager::lastError() const { return m_error; }
