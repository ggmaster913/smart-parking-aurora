#include "booking/bookingservice.h"

#include "core/sessioncontext.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtGlobal>
#include <QDebug>

#include <limits>

namespace {

const char Currency[] = "RUB";
const int MaxIdLength = 128;

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
                       QStringLiteral("Не удалось сохранить данные. Попробуйте ещё раз."));
}

QVariantMap invalidDuration()
{
    return errorResult(QStringLiteral("INVALID_INPUT"),
                       QStringLiteral("Длительность — от 15 минут до 24 часов, шаг 15 минут."));
}

QVariantMap notAuthenticated()
{
    return errorResult(QStringLiteral("NOT_AUTHENTICATED"),
                       QStringLiteral("Войдите, чтобы оплатить парковку."));
}

QVariantMap idempotencyConflict()
{
    return errorResult(QStringLiteral("IDEMPOTENCY_CONFLICT"),
                       QStringLiteral("Операция уже выполнена с другими параметрами. "
                                      "Повторите действие заново."));
}

QVariantMap invalidOperationId()
{
    return errorResult(QStringLiteral("INVALID_INPUT"),
                       QStringLiteral("Не удалось подтвердить операцию. Повторите действие."));
}

QVariantMap amountOutOfRange()
{
    return errorResult(QStringLiteral("INVALID_INPUT"),
                       QStringLiteral("Сумма превышает допустимый предел. "
                                      "Выберите меньшую длительность."));
}

bool isValidId(const QString &id)
{
    return !id.trimmed().isEmpty() && id.size() <= MaxIdLength;
}

// JSON хранит числа как double; возвращаем целые поля как qint64,
// чтобы повторный ответ совпадал с исходным.
QVariant normalizeNumbers(const QVariant &value)
{
    if (value.type() == QVariant::Map) {
        QVariantMap map = value.toMap();
        for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it)
            it.value() = normalizeNumbers(it.value());
        return map;
    }
    if (value.type() == QVariant::Double) {
        const double d = value.toDouble();
        const qint64 i = static_cast<qint64>(d);
        if (static_cast<double>(i) == d)
            return QVariant(static_cast<qlonglong>(i));
    }
    if (value.type() == QVariant::Int || value.type() == QVariant::LongLong
        || value.type() == QVariant::UInt || value.type() == QVariant::ULongLong) {
        return QVariant(value.toLongLong());
    }
    return value;
}

QString toJson(const QVariantMap &map)
{
    return QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact));
}

QVariantMap fromJson(const QString &json, bool *ok)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    *ok = error.error == QJsonParseError::NoError && doc.isObject();
    if (!*ok)
        return QVariantMap();
    return normalizeNumbers(doc.object().toVariantMap()).toMap();
}

QString newId()
{
    QString id = QUuid::createUuid().toString(); // "{xxxxxxxx-...}" в Qt 5.6
    id.remove(QLatin1Char('{'));
    id.remove(QLatin1Char('}'));
    return id;
}

// Откатывает транзакцию, если commit() не был вызван.
class Transaction
{
public:
    explicit Transaction(QSqlDatabase &db) : m_db(db), m_open(db.transaction()) {}
    ~Transaction()
    {
        if (m_open)
            m_db.rollback();
    }
    bool isOpen() const { return m_open; }
    bool commit()
    {
        if (!m_open)
            return false;
        m_open = false;
        if (m_db.commit())
            return true;
        m_db.rollback();
        return false;
    }

private:
    QSqlDatabase &m_db;
    bool m_open;
};

void logSqlError(const char *where, const QSqlQuery &query)
{
    qWarning() << "BookingService:" << where << query.lastError().text();
}

enum class Lookup { Found, Missing, Failed };

struct StoredOperation
{
    QString userId;
    QString kind;
    QString requestJson;
    QString responseJson;
};

Lookup findOperation(QSqlDatabase &db, const QString &operationId, StoredOperation *out)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT user_id, kind, request_json, response_json "
                             "FROM payment_operations WHERE id = ?"));
    q.addBindValue(operationId);
    if (!q.exec()) {
        logSqlError("findOperation", q);
        return Lookup::Failed;
    }
    if (!q.next())
        return Lookup::Missing;
    out->userId = q.value(0).toString();
    out->kind = q.value(1).toString();
    out->requestJson = q.value(2).toString();
    out->responseJson = q.value(3).toString();
    return Lookup::Found;
}

// Повтор операции: тот же пользователь, вид и нормализованный запрос —
// сохранённый ответ; иначе конфликт без раскрытия чужих данных.
QVariantMap replayOrConflict(const StoredOperation &stored, const QString &userId,
                             const QString &kind, const QString &requestJson)
{
    if (stored.userId != userId || stored.kind != kind || stored.requestJson != requestJson)
        return idempotencyConflict();
    bool ok = false;
    const QVariantMap response = fromJson(stored.responseJson, &ok);
    if (!ok)
        return storageError();
    return response;
}

Lookup userExists(QSqlDatabase &db, const QString &userId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM users WHERE id = ?"));
    q.addBindValue(userId);
    if (!q.exec()) {
        logSqlError("userExists", q);
        return Lookup::Failed;
    }
    return q.next() ? Lookup::Found : Lookup::Missing;
}

Lookup activeSessionExists(QSqlDatabase &db, const QString &column, const QString &value)
{
    // column — только литерал из этого файла, значение параметризовано
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM sessions WHERE status = 'active' AND ")
              + column + QStringLiteral(" = ? LIMIT 1"));
    q.addBindValue(value);
    if (!q.exec()) {
        logSqlError("activeSessionExists", q);
        return Lookup::Failed;
    }
    return q.next() ? Lookup::Found : Lookup::Missing;
}

bool insertOperation(QSqlDatabase &db, const QString &operationId, const QString &userId,
                     const QString &sessionId, const QString &kind, const QString &requestJson,
                     const QString &responseJson, qint64 amount, qint64 nowUtc)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO payment_operations (id, user_id, session_id, kind, request_json, "
        "response_json, amount_kopecks, created_at_utc) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(operationId);
    q.addBindValue(userId);
    q.addBindValue(sessionId);
    q.addBindValue(kind);
    q.addBindValue(requestJson);
    q.addBindValue(responseJson);
    q.addBindValue(static_cast<qlonglong>(amount));
    q.addBindValue(static_cast<qlonglong>(nowUtc));
    if (!q.exec()) {
        logSqlError("insertOperation", q);
        return false;
    }
    return true;
}

const char SessionSelect[] =
    "SELECT s.id, sp.zone_id, s.space_id, s.vehicle_id, s.plate_snapshot, s.starts_at_utc, "
    "s.expires_at_utc, s.amount_kopecks, s.status, s.user_id, s.rate_kopecks_per_hour "
    "FROM sessions s JOIN spaces sp ON sp.id = s.space_id ";

QVariantMap sessionFromQuery(const QSqlQuery &q)
{
    const QString id = q.value(0).toString();
    QVariantMap s;
    s.insert(QStringLiteral("id"), id);
    s.insert(QStringLiteral("zoneId"), q.value(1).toString());
    s.insert(QStringLiteral("spaceId"), q.value(2).toString());
    s.insert(QStringLiteral("vehicleId"), q.value(3).toString());
    s.insert(QStringLiteral("plate"), q.value(4).toString());
    s.insert(QStringLiteral("startsAtUtc"), q.value(5).toLongLong());
    s.insert(QStringLiteral("expiresAtUtc"), q.value(6).toLongLong());
    s.insert(QStringLiteral("amountKopecks"), q.value(7).toLongLong());
    s.insert(QStringLiteral("currency"), QString::fromLatin1(Currency));
    s.insert(QStringLiteral("status"), q.value(8).toString());
    s.insert(QStringLiteral("ticketNumber"), QStringLiteral("DEMO-") + id);
    s.insert(QStringLiteral("paymentIsDemo"), true);
    return s;
}

} // namespace

const int BookingService::MinDurationMinutes;
const int BookingService::MaxDurationMinutes;
const int BookingService::DurationStepMinutes;
const qint64 BookingService::MaxAmountKopecks;

BookingService::BookingService(QSqlDatabase db, SessionContext *context, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_context(context)
{
    if (m_context)
        connect(m_context, &SessionContext::userChanged, this, &BookingService::refresh);
}

QVariantMap BookingService::activeSession() const
{
    return m_activeSession;
}

QVariantList BookingService::history() const
{
    // История поездок — отдельная задача этапа 4, пока пусто.
    return m_history;
}

bool BookingService::isValidDuration(int minutes)
{
    return minutes >= MinDurationMinutes && minutes <= MaxDurationMinutes
           && minutes % DurationStepMinutes == 0;
}

qint64 BookingService::priceKopecks(qint64 rateKopecksPerHour, int minutes)
{
    if (rateKopecksPerHour < 0 || minutes < 0)
        return -1;
    if (minutes > 0 && rateKopecksPerHour > std::numeric_limits<qint64>::max() / minutes)
        return -1;
    const qint64 product = rateKopecksPerHour * static_cast<qint64>(minutes);
    const qint64 amount = product / 60 + (product % 60 != 0 ? 1 : 0);
    return amount <= MaxAmountKopecks ? amount : -1;
}

qint64 BookingService::currentUtc() const
{
    return QDateTime::currentMSecsSinceEpoch() / 1000;
}

QVariantMap BookingService::quote(const QString &zoneId, int durationMinutes) const
{
    if (!isValidDuration(durationMinutes))
        return invalidDuration();
    if (!isValidId(zoneId))
        return errorResult(QStringLiteral("INVALID_INPUT"), QStringLiteral("Не выбрана зона."));

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT rate_kopecks_per_hour FROM zones WHERE id = ?"));
    q.addBindValue(zoneId);
    if (!q.exec()) {
        logSqlError("quote", q);
        return storageError();
    }
    if (!q.next())
        return errorResult(QStringLiteral("NOT_FOUND"), QStringLiteral("Зона не найдена."));

    const qint64 amount = priceKopecks(q.value(0).toLongLong(), durationMinutes);
    if (amount < 0)
        return amountOutOfRange();

    QVariantMap data;
    data.insert(QStringLiteral("amountKopecks"), static_cast<qlonglong>(amount));
    data.insert(QStringLiteral("durationMinutes"), static_cast<qlonglong>(durationMinutes));
    data.insert(QStringLiteral("currency"), QString::fromLatin1(Currency));
    return okResult(data);
}

QVariantMap BookingService::book(const QString &zoneId, const QString &spaceId,
                                 const QString &vehicleId, int durationMinutes,
                                 const QString &operationId)
{
    if (!isValidId(operationId))
        return invalidOperationId();
    const QString userId = m_context ? m_context->userId() : QString();
    if (userId.isEmpty())
        return notAuthenticated();

    QVariantMap request;
    request.insert(QStringLiteral("kind"), QStringLiteral("book"));
    request.insert(QStringLiteral("zoneId"), zoneId);
    request.insert(QStringLiteral("spaceId"), spaceId);
    request.insert(QStringLiteral("vehicleId"), vehicleId);
    request.insert(QStringLiteral("durationMinutes"), durationMinutes);
    const QString requestJson = toJson(request);

    const qint64 now = currentUtc();
    QVariantMap result;
    {
        Transaction tx(m_db);
        if (!tx.isOpen())
            return storageError();
        if (!completeExpired(now))
            return storageError();

        StoredOperation stored;
        switch (findOperation(m_db, operationId, &stored)) {
        case Lookup::Failed:
            return storageError();
        case Lookup::Found:
            result = replayOrConflict(stored, userId, QStringLiteral("book"), requestJson);
            if (!tx.commit())
                return storageError();
            setActiveSession(loadActiveSession(userId));
            return result;
        case Lookup::Missing:
            break;
        }

        // Предметные проверки — после ключа: известный ключ с иными
        // параметрами, даже неверными, всегда даёт IDEMPOTENCY_CONFLICT.
        if (!isValidDuration(durationMinutes))
            return invalidDuration();
        if (!isValidId(zoneId) || !isValidId(spaceId) || !isValidId(vehicleId)) {
            return errorResult(QStringLiteral("INVALID_INPUT"),
                               QStringLiteral("Выберите зону, место и автомобиль."));
        }

        switch (userExists(m_db, userId)) {
        case Lookup::Failed: return storageError();
        case Lookup::Missing: return notAuthenticated();
        case Lookup::Found: break;
        }

        QString plate;
        {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral("SELECT user_id, plate FROM vehicles WHERE id = ?"));
            q.addBindValue(vehicleId);
            if (!q.exec()) {
                logSqlError("book vehicle", q);
                return storageError();
            }
            if (!q.next())
                return errorResult(QStringLiteral("NOT_FOUND"),
                                   QStringLiteral("Автомобиль не найден."));
            if (q.value(0).toString() != userId)
                return errorResult(QStringLiteral("NOT_OWNER"),
                                   QStringLiteral("Этот автомобиль не принадлежит вам."));
            plate = q.value(1).toString();
        }

        qint64 rate = 0;
        {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral(
                "SELECT sp.mock_occupied, z.rate_kopecks_per_hour FROM spaces sp "
                "JOIN zones z ON z.id = sp.zone_id WHERE sp.id = ? AND sp.zone_id = ?"));
            q.addBindValue(spaceId);
            q.addBindValue(zoneId);
            if (!q.exec()) {
                logSqlError("book space", q);
                return storageError();
            }
            if (!q.next())
                return errorResult(QStringLiteral("NOT_FOUND"),
                                   QStringLiteral("Место не найдено в выбранной зоне."));
            if (q.value(0).toInt() != 0)
                return errorResult(QStringLiteral("SPACE_UNAVAILABLE"),
                                   QStringLiteral("Место занято. Выберите другое."));
            rate = q.value(1).toLongLong();
        }

        switch (activeSessionExists(m_db, QStringLiteral("user_id"), userId)) {
        case Lookup::Failed: return storageError();
        case Lookup::Found:
            return errorResult(QStringLiteral("ACTIVE_SESSION_EXISTS"),
                               QStringLiteral("У вас уже есть активная парковка."));
        case Lookup::Missing: break;
        }
        switch (activeSessionExists(m_db, QStringLiteral("space_id"), spaceId)) {
        case Lookup::Failed: return storageError();
        case Lookup::Found:
            return errorResult(QStringLiteral("SPACE_UNAVAILABLE"),
                               QStringLiteral("Место уже занято. Выберите другое."));
        case Lookup::Missing: break;
        }

        const qint64 amount = priceKopecks(rate, durationMinutes);
        if (amount < 0)
            return amountOutOfRange();
        const QString sessionId = newId();
        const qint64 expires = now + static_cast<qint64>(durationMinutes) * 60;
        {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral(
                "INSERT INTO sessions (id, user_id, vehicle_id, space_id, starts_at_utc, "
                "expires_at_utc, rate_kopecks_per_hour, amount_kopecks, status, plate_snapshot) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'active', ?)"));
            q.addBindValue(sessionId);
            q.addBindValue(userId);
            q.addBindValue(vehicleId);
            q.addBindValue(spaceId);
            q.addBindValue(static_cast<qlonglong>(now));
            q.addBindValue(static_cast<qlonglong>(expires));
            q.addBindValue(static_cast<qlonglong>(rate));
            q.addBindValue(static_cast<qlonglong>(amount));
            q.addBindValue(plate);
            if (!q.exec()) {
                logSqlError("book insert session", q);
                return storageError();
            }
        }

        QVariantMap session;
        session.insert(QStringLiteral("id"), sessionId);
        session.insert(QStringLiteral("zoneId"), zoneId);
        session.insert(QStringLiteral("spaceId"), spaceId);
        session.insert(QStringLiteral("vehicleId"), vehicleId);
        session.insert(QStringLiteral("plate"), plate);
        session.insert(QStringLiteral("startsAtUtc"), static_cast<qlonglong>(now));
        session.insert(QStringLiteral("expiresAtUtc"), static_cast<qlonglong>(expires));
        session.insert(QStringLiteral("amountKopecks"), static_cast<qlonglong>(amount));
        session.insert(QStringLiteral("chargedKopecks"), static_cast<qlonglong>(amount));
        session.insert(QStringLiteral("durationMinutes"), static_cast<qlonglong>(durationMinutes));
        session.insert(QStringLiteral("currency"), QString::fromLatin1(Currency));
        session.insert(QStringLiteral("status"), QStringLiteral("active"));
        session.insert(QStringLiteral("ticketNumber"), QStringLiteral("DEMO-") + sessionId);
        session.insert(QStringLiteral("paymentIsDemo"), true);

        const QString responseJson = toJson(okResult(session));
        if (!insertOperation(m_db, operationId, userId, sessionId, QStringLiteral("book"),
                             requestJson, responseJson, amount, now)) {
            return storageError();
        }
        bool parsed = false;
        result = fromJson(responseJson, &parsed);
        if (!parsed || !tx.commit())
            return storageError();
    }
    setActiveSession(loadActiveSession(userId));
    return result;
}

QVariantMap BookingService::extend(const QString &sessionId, int additionalMinutes,
                                   const QString &operationId)
{
    if (!isValidId(operationId))
        return invalidOperationId();
    const QString userId = m_context ? m_context->userId() : QString();
    if (userId.isEmpty())
        return notAuthenticated();

    QVariantMap request;
    request.insert(QStringLiteral("kind"), QStringLiteral("extend"));
    request.insert(QStringLiteral("sessionId"), sessionId);
    request.insert(QStringLiteral("additionalMinutes"), additionalMinutes);
    const QString requestJson = toJson(request);

    const qint64 now = currentUtc();
    QVariantMap result;
    {
        Transaction tx(m_db);
        if (!tx.isOpen())
            return storageError();
        // Истёкшие сессии завершаем до продления; изменение фиксируется даже при отказе.
        if (!completeExpired(now))
            return storageError();

        StoredOperation stored;
        Lookup found = findOperation(m_db, operationId, &stored);
        if (found == Lookup::Failed)
            return storageError();
        if (found == Lookup::Found) {
            result = replayOrConflict(stored, userId, QStringLiteral("extend"), requestJson);
            if (!tx.commit())
                return storageError();
            setActiveSession(loadActiveSession(userId));
            return result;
        }

        // Предметные проверки — после ключа (см. book).
        if (!isValidDuration(additionalMinutes))
            return invalidDuration();
        if (!isValidId(sessionId))
            return errorResult(QStringLiteral("INVALID_INPUT"),
                               QStringLiteral("Не выбрана парковка для продления."));

        QVariantMap session;
        qint64 rate = 0;
        {
            QSqlQuery q(m_db);
            q.prepare(QString::fromLatin1(SessionSelect) + QStringLiteral("WHERE s.id = ?"));
            q.addBindValue(sessionId);
            if (!q.exec()) {
                logSqlError("extend select", q);
                return storageError();
            }
            if (!q.next())
                return errorResult(QStringLiteral("NOT_FOUND"),
                                   QStringLiteral("Парковка не найдена."));
            if (q.value(9).toString() != userId)
                return errorResult(QStringLiteral("NOT_OWNER"),
                                   QStringLiteral("Продлить можно только свою парковку."));
            session = sessionFromQuery(q);
            rate = q.value(10).toLongLong();
        }

        bool expiredNow = false;
        if (session.value(QStringLiteral("status")).toString() != QLatin1String("active")
            || session.value(QStringLiteral("expiresAtUtc")).toLongLong() <= now) {
            expiredNow = true;
        }
        if (expiredNow) {
            result = errorResult(QStringLiteral("SESSION_EXPIRED"),
                                 QStringLiteral("Время парковки уже закончилось."));
            if (!tx.commit())
                return storageError();
            setActiveSession(loadActiveSession(userId));
            return result;
        }

        const qint64 charged = priceKopecks(rate, additionalMinutes);
        if (charged < 0)
            return amountOutOfRange();
        const qint64 newExpires = session.value(QStringLiteral("expiresAtUtc")).toLongLong()
                                  + static_cast<qint64>(additionalMinutes) * 60;
        const qint64 previous = session.value(QStringLiteral("amountKopecks")).toLongLong();
        if (previous < 0 || charged > MaxAmountKopecks - previous)
            return amountOutOfRange();
        const qint64 total = previous + charged;
        {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral(
                "UPDATE sessions SET expires_at_utc = ?, amount_kopecks = ? "
                "WHERE id = ? AND user_id = ? AND status = 'active'"));
            q.addBindValue(static_cast<qlonglong>(newExpires));
            q.addBindValue(static_cast<qlonglong>(total));
            q.addBindValue(sessionId);
            q.addBindValue(userId);
            if (!q.exec() || q.numRowsAffected() != 1) {
                logSqlError("extend update", q);
                return storageError();
            }
        }

        session.insert(QStringLiteral("expiresAtUtc"), static_cast<qlonglong>(newExpires));
        session.insert(QStringLiteral("amountKopecks"), static_cast<qlonglong>(total));
        session.insert(QStringLiteral("chargedKopecks"), static_cast<qlonglong>(charged));
        session.insert(QStringLiteral("additionalMinutes"),
                       static_cast<qlonglong>(additionalMinutes));

        const QString responseJson = toJson(okResult(session));
        if (!insertOperation(m_db, operationId, userId, sessionId, QStringLiteral("extend"),
                             requestJson, responseJson, charged, now)) {
            return storageError();
        }
        bool parsed = false;
        result = fromJson(responseJson, &parsed);
        if (!parsed || !tx.commit())
            return storageError();
    }
    setActiveSession(loadActiveSession(userId));
    return result;
}

QVariantMap BookingService::finish(const QString &sessionId)
{
    const QString userId = m_context ? m_context->userId() : QString();
    if (userId.isEmpty())
        return notAuthenticated();
    if (!isValidId(sessionId))
        return errorResult(QStringLiteral("INVALID_INPUT"),
                           QStringLiteral("Не выбрана парковка для завершения."));

    const qint64 now = currentUtc();
    QVariantMap session;
    {
        Transaction tx(m_db);
        if (!tx.isOpen())
            return storageError();
        if (!completeExpired(now))
            return storageError();

        {
            QSqlQuery q(m_db);
            q.prepare(QString::fromLatin1(SessionSelect) + QStringLiteral("WHERE s.id = ?"));
            q.addBindValue(sessionId);
            if (!q.exec()) {
                logSqlError("finish select", q);
                return storageError();
            }
            if (!q.next())
                return errorResult(QStringLiteral("NOT_FOUND"),
                                   QStringLiteral("Парковка не найдена."));
            if (q.value(9).toString() != userId)
                return errorResult(QStringLiteral("NOT_OWNER"),
                                   QStringLiteral("Завершить можно только свою парковку."));
            session = sessionFromQuery(q);
        }

        if (session.value(QStringLiteral("status")).toString() == QLatin1String("active")) {
            // CHECK схемы: expires_at_utc > starts_at_utc, поэтому не раньше начала + 1 с.
            const qint64 startsAt = session.value(QStringLiteral("startsAtUtc")).toLongLong();
            const qint64 finishedAt = qMax(now, startsAt + 1);
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral(
                "UPDATE sessions SET status = 'completed', expires_at_utc = ? "
                "WHERE id = ? AND user_id = ? AND status = 'active'"));
            q.addBindValue(static_cast<qlonglong>(finishedAt));
            q.addBindValue(sessionId);
            q.addBindValue(userId);
            if (!q.exec() || q.numRowsAffected() != 1) {
                logSqlError("finish update", q);
                return storageError();
            }
            session.insert(QStringLiteral("status"), QStringLiteral("completed"));
            session.insert(QStringLiteral("expiresAtUtc"), static_cast<qlonglong>(finishedAt));
        }
        if (!tx.commit())
            return storageError();
    }
    setActiveSession(loadActiveSession(userId));
    return okResult(session);
}

void BookingService::refresh()
{
    const qint64 now = currentUtc();
    {
        Transaction tx(m_db);
        if (tx.isOpen() && completeExpired(now))
            tx.commit();
    }
    const QString userId = m_context ? m_context->userId() : QString();
    setActiveSession(userId.isEmpty() ? QVariantMap() : loadActiveSession(userId));
}

bool BookingService::completeExpired(qint64 nowUtc)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE sessions SET status = 'completed' "
                             "WHERE status = 'active' AND expires_at_utc <= ?"));
    q.addBindValue(static_cast<qlonglong>(nowUtc));
    if (!q.exec()) {
        logSqlError("completeExpired", q);
        return false;
    }
    return true;
}

QVariantMap BookingService::loadActiveSession(const QString &userId) const
{
    QSqlQuery q(m_db);
    q.prepare(QString::fromLatin1(SessionSelect)
              + QStringLiteral("WHERE s.user_id = ? AND s.status = 'active' "
                               "AND s.expires_at_utc > ? LIMIT 1"));
    q.addBindValue(userId);
    q.addBindValue(static_cast<qlonglong>(currentUtc()));
    if (!q.exec()) {
        logSqlError("loadActiveSession", q);
        return QVariantMap();
    }
    if (!q.next())
        return QVariantMap();
    return sessionFromQuery(q);
}

void BookingService::setActiveSession(const QVariantMap &session)
{
    if (session == m_activeSession)
        return;
    m_activeSession = session;
    emit activeSessionChanged();
}
