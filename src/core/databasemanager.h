#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H
#include <QSqlDatabase>
#include <QString>

class DatabaseManager {
public:
    explicit DatabaseManager(const QString &databasePath = QString(),
                             const QString &migrationPath = QString());
    ~DatabaseManager();
    bool open();
    QSqlDatabase connection() const;
    QString lastError() const;
private:
    Q_DISABLE_COPY(DatabaseManager)
    bool fail(const QString &message);
    QString m_path;
    QString m_migrationPath;
    QString m_connectionName;
    QString m_error;
    QSqlDatabase m_database;
};
#endif
