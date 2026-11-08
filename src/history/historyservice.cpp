// HistoryService (#22): read-only история парковок и расходы, контракт docs/history.md.
#include "history/historyservice.h"

#include "core/sessioncontext.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>

namespace {

// SQL вынесен в константы: tests/history/test_history_sql.py выполняет эти же строки
// на настоящей migrations/001_initial.sql. Только SELECT, параметры — :user_id.
// HISTORY_SQL_BEGIN entries
const char EntriesSql[] =
    "SELECT s.id, sp.zone_id, z.name, z.address, s.plate_snapshot, "
    "s.starts_at_utc, s.expires_at_utc, s.amount_kopecks, s.status "
    "FROM sessions s "
    "JOIN spaces sp ON sp.id = s.space_id "
    "JOIN zones z ON z.id = sp.zone_id "
    "WHERE s.user_id = :user_id "
    "ORDER BY s.starts_at_utc DESC, s.id ASC "
    "LIMIT 100";
// HISTORY_SQL_END
// Итоги по ВСЕМ сессиям пользователя: amount_kopecks сессии уже включает продления,
// поэтому payment_operations повторно не складываются. Сумма считается в C++ с проверкой предела.
// HISTORY_SQL_BEGIN amounts
const char AmountsSql[] =
    "SELECT s.amount_kopecks FROM sessions s WHERE s.user_id = :user_id";
// HISTORY_SQL_END

QVariantMap okResult(const QVariantMap &data)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("errorCode"), QString());
    result.insert(QStringLiteral("message"), QString());
    result.insert(QStringLiteral("data"), data);
    return result;
}

QVariantMap errorResult(const QString &code, const QString &message)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("errorCode"), code);
    result.insert(QStringLiteral("message"), message);
    result.insert(QStringLiteral("data"), QVariantMap());
    return result;
}

QVariantMap storageError()
{
    return errorResult(QStringLiteral("STORAGE_ERROR"),
                       QStringLiteral("Не удалось загрузить историю. Попробуйте ещё раз."));
}

} // namespace

// Определения для ODR-использования (например, QCOMPARE берёт аргументы по ссылке), C++11/14.
const int HistoryService::MaxEntries;
const qint64 HistoryService::MaxTotalKopecks;

HistoryService::HistoryService(QSqlDatabase db, SessionContext *context, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_context(context)
    , m_totalKopecks(0)
    , m_totalSessions(0)
{
    if (m_context)
        connect(m_context, &SessionContext::userChanged, this, &HistoryService::onUserChanged);
}

QVariantList HistoryService::entries() const { return m_entries; }
qint64 HistoryService::totalKopecks() const { return m_totalKopecks; }
int HistoryService::totalSessions() const { return m_totalSessions; }

qint64 HistoryService::currentUtc() const
{
    return static_cast<qint64>(QDateTime::currentMSecsSinceEpoch() / 1000);
}

void HistoryService::clear()
{
    const bool changed = !m_entries.isEmpty() || m_totalKopecks != 0 || m_totalSessions != 0;
    m_entries.clear();
    m_totalKopecks = 0;
    m_totalSessions = 0;
    if (changed)
        emit historyChanged();
}

void HistoryService::onUserChanged()
{
    // Сначала убрать данные прежнего пользователя, затем загрузить нового.
    clear();
    refresh();
}

QVariantMap HistoryService::refresh()
{
    const QString userId = m_context ? m_context->userId() : QString();
    if (userId.isEmpty()) {
        clear();
        return errorResult(QStringLiteral("NOT_AUTHENTICATED"),
                           QStringLiteral("Войдите, чтобы увидеть историю парковок."));
    }
    if (!m_db.isOpen()) {
        clear();
        return storageError();
    }

    // 1. Итоги по всем сессиям пользователя.
    qint64 total = 0;
    int sessions = 0;
    {
        QSqlQuery q(m_db);
        q.setForwardOnly(true);
        if (!q.prepare(QString::fromLatin1(AmountsSql))) {
            clear();
            return storageError();
        }
        q.bindValue(QStringLiteral(":user_id"), userId);
        if (!q.exec()) {
            clear();
            return storageError();
        }
        while (q.next()) {
            bool ok = false;
            const qint64 amount = q.value(0).toLongLong(&ok);
            if (!ok || amount < 0 || amount > MaxTotalKopecks - total) {
                clear();
                return errorResult(QStringLiteral("STORAGE_ERROR"),
                                   QStringLiteral("Сумма расходов превышает допустимый предел."));
            }
            total += amount;
            ++sessions;
        }
    }

    // 2. Последние MaxEntries сессий для списка.
    QVariantList list;
    {
        QSqlQuery q(m_db);
        q.setForwardOnly(true);
        if (!q.prepare(QString::fromLatin1(EntriesSql))) {
            clear();
            return storageError();
        }
        q.bindValue(QStringLiteral(":user_id"), userId);
        if (!q.exec()) {
            clear();
            return storageError();
        }
        const qint64 now = currentUtc();
        while (q.next()) {
            const QString id = q.value(0).toString();
            const qint64 expires = q.value(6).toLongLong();
            QString status = q.value(8).toString();
            // Истёкшая active показывается завершённой; сама БД здесь не меняется.
            if (status == QLatin1String("active") && expires <= now)
                status = QStringLiteral("completed");

            QVariantMap entry;
            entry.insert(QStringLiteral("id"), id);
            entry.insert(QStringLiteral("zoneId"), q.value(1).toString());
            entry.insert(QStringLiteral("zoneName"), q.value(2).toString());
            entry.insert(QStringLiteral("address"), q.value(3).toString());
            entry.insert(QStringLiteral("plate"), q.value(4).toString());
            entry.insert(QStringLiteral("startsAtUtc"), static_cast<qlonglong>(q.value(5).toLongLong()));
            entry.insert(QStringLiteral("expiresAtUtc"), static_cast<qlonglong>(expires));
            entry.insert(QStringLiteral("amountKopecks"), static_cast<qlonglong>(q.value(7).toLongLong()));
            entry.insert(QStringLiteral("currency"), QStringLiteral("RUB"));
            entry.insert(QStringLiteral("status"), status);
            entry.insert(QStringLiteral("ticketNumber"), QStringLiteral("DEMO-") + id);
            entry.insert(QStringLiteral("paymentIsDemo"), true);
            list.append(entry);
        }
    }

    // Пользователь мог смениться во время чтения: чужие данные не публикуем.
    if (!m_context || m_context->userId() != userId) {
        clear();
        return errorResult(QStringLiteral("NOT_AUTHENTICATED"),
                           QStringLiteral("Войдите, чтобы увидеть историю парковок."));
    }

    m_entries = list;
    m_totalKopecks = total;
    m_totalSessions = sessions;
    emit historyChanged();

    QVariantMap data;
    data.insert(QStringLiteral("totalKopecks"), static_cast<qlonglong>(total));
    data.insert(QStringLiteral("totalSessions"), sessions);
    data.insert(QStringLiteral("shownSessions"), list.size());
    return okResult(data);
}
