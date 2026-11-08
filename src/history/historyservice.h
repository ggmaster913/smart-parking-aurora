// HistoryService — история парковок и расходы текущего пользователя (#22).
// Только чтение SQLite: сервис не выполняет INSERT/UPDATE/DELETE.
// Деньги — целые копейки RUB, время — Unix UTC в секундах. Контракт: docs/history.md.
#ifndef HISTORYSERVICE_H
#define HISTORYSERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class SessionContext;

class HistoryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY historyChanged)
    Q_PROPERTY(qint64 totalKopecks READ totalKopecks NOTIFY historyChanged)
    Q_PROPERTY(int totalSessions READ totalSessions NOTIFY historyChanged)

public:
    HistoryService(QSqlDatabase db, SessionContext *context, QObject *parent = nullptr);

    QVariantList entries() const;
    qint64 totalKopecks() const;
    int totalSessions() const;

    // Перечитывает историю текущего пользователя. Result: {ok, errorCode, message, data}.
    // data: {totalKopecks, totalSessions, shownSessions}.
    Q_INVOKABLE QVariantMap refresh();

    static const int MaxEntries = 100;
    // Предел точного целого в QML/JSON: 2^53 − 1 коп.
    static const qint64 MaxTotalKopecks = 9007199254740991LL;

signals:
    void historyChanged();

protected:
    // Текущее время UTC в секундах. Переопределяется только в тестах.
    virtual qint64 currentUtc() const;

private slots:
    void onUserChanged();

private:
    void clear();

    QSqlDatabase m_db;
    SessionContext *m_context;
    QVariantList m_entries;
    qint64 m_totalKopecks;
    int m_totalSessions;
};

#endif // HISTORYSERVICE_H
