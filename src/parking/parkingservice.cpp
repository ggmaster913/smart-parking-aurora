// SPDX-License-Identifier: MIT
#include "parkingservice.h"

#include "parkinggeo.h"

#include <QDateTime>
#include <QDebug>
#include <QtGlobal>
#include <QSqlError>
#include <QSqlQuery>

namespace {

QVariantMap makeResult(bool ok, const QString &errorCode, const QString &message,
                       const QVariantMap &data = QVariantMap())
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("errorCode"), errorCode);
    result.insert(QStringLiteral("message"), message);
    result.insert(QStringLiteral("data"), ok ? data : QVariantMap());
    return result;
}

// Место свободно, если не занято в мок-данных и не имеет неистёкшей активной сессии.
const char kAvailableExpression[] =
        "(s.mock_occupied = 0 AND NOT EXISTS ("
        " SELECT 1 FROM sessions se WHERE se.space_id = s.id"
        " AND se.status = 'active' AND se.expires_at_utc > :now))";

} // namespace

ParkingService::ParkingService(QSqlDatabase database, SessionContext *session, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_session(session)
    , m_hasPosition(false)
    , m_positionIsDemo(false)
    , m_latitude(0)
    , m_longitude(0)
    , m_nextChangeUtc(-1)
{
    m_expiryTimer.setSingleShot(true);
    connect(&m_expiryTimer, &QTimer::timeout, this, &ParkingService::refresh);
}

QVariantList ParkingService::zones() const
{
    return m_zones;
}

bool ParkingService::hasPosition() const
{
    return m_hasPosition;
}

bool ParkingService::positionIsDemo() const
{
    return m_positionIsDemo;
}

qint64 ParkingService::nowUtc() const
{
    return QDateTime::currentMSecsSinceEpoch() / 1000;
}

QVariantMap ParkingService::setPosition(double latitude, double longitude, bool isDemo)
{
    if (!ParkingGeo::isValidCoordinate(latitude, longitude)) {
        return makeResult(false, QStringLiteral("INVALID_INPUT"),
                          QStringLiteral("Некорректные координаты: широта должна быть от -90 до 90, "
                                         "долгота — от -180 до 180"));
    }

    const bool changed = !m_hasPosition || m_positionIsDemo != isDemo
            || m_latitude != latitude || m_longitude != longitude;
    m_latitude = latitude;
    m_longitude = longitude;
    m_positionIsDemo = isDemo;
    m_hasPosition = true;

    if (changed) {
        emit positionChanged();
        // Новая позиция — повод перечитать доступность: сессии могли истечь.
        reloadFromDatabase();
        rebuildZones();
        emit zonesChanged();
    }

    QVariantMap data;
    data.insert(QStringLiteral("latitude"), latitude);
    data.insert(QStringLiteral("longitude"), longitude);
    data.insert(QStringLiteral("isDemo"), isDemo);
    return makeResult(true, QString(), QString(), data);
}

QVariantMap ParkingService::zoneDetails(const QString &zoneId) const
{
    const QString id = zoneId.trimmed();
    if (id.isEmpty()) {
        return makeResult(false, QStringLiteral("INVALID_INPUT"),
                          QStringLiteral("Не указан идентификатор парковки"));
    }

    QVariantList found;
    if (!loadZones(id, &found)) {
        return makeResult(false, QStringLiteral("STORAGE_ERROR"),
                          QStringLiteral("Не удалось прочитать данные парковки"));
    }
    if (found.isEmpty()) {
        return makeResult(false, QStringLiteral("NOT_FOUND"),
                          QStringLiteral("Парковка не найдена"));
    }
    return makeResult(true, QString(), QString(), withDistance(found.first().toMap()));
}

QVariantList ParkingService::spaces(const QString &zoneId) const
{
    QVariantList result;
    const QString id = zoneId.trimmed();
    if (id.isEmpty() || !m_database.isOpen())
        return result;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT s.id, s.zone_id, s.label, ")
                  + QLatin1String(kAvailableExpression)
                  + QStringLiteral(" AS available FROM spaces s WHERE s.zone_id = :zoneId"
                                   " ORDER BY s.label, s.id"));
    query.bindValue(QStringLiteral(":now"), nowUtc());
    query.bindValue(QStringLiteral(":zoneId"), id);
    if (!query.exec()) {
        qWarning() << "ParkingService::spaces:" << query.lastError().text();
        return QVariantList();
    }
    while (query.next()) {
        QVariantMap space;
        space.insert(QStringLiteral("id"), query.value(0).toString());
        space.insert(QStringLiteral("zoneId"), query.value(1).toString());
        space.insert(QStringLiteral("label"), query.value(2).toString());
        space.insert(QStringLiteral("available"), query.value(3).toInt() != 0);
        result.append(space);
    }
    return result;
}

void ParkingService::refresh()
{
    if (!reloadFromDatabase())
        return;
    rebuildZones();
    emit zonesChanged();
}

qint64 ParkingService::nextAvailabilityChangeUtc() const
{
    return m_nextChangeUtc;
}

int ParkingService::scheduledRefreshMs() const
{
    return m_expiryTimer.isActive() ? m_expiryTimer.interval() : -1;
}

bool ParkingService::reloadFromDatabase()
{
    QVariantList loaded;
    if (!loadZones(QString(), &loaded)) {
        // Оставляем последний успешно прочитанный каталог, ошибку пишем в журнал.
        return false;
    }
    m_baseZones = loaded;
    scheduleExpiryRefresh();
    return true;
}

void ParkingService::scheduleExpiryRefresh()
{
    m_expiryTimer.stop();
    m_nextChangeUtc = -1;

    const qint64 now = nowUtc();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT MIN(expires_at_utc) FROM sessions"
                                 " WHERE status = 'active' AND expires_at_utc > :now"));
    query.bindValue(QStringLiteral(":now"), now);
    if (!query.exec() || !query.next() || query.value(0).isNull()) {
        if (query.lastError().isValid())
            qWarning() << "ParkingService: ошибка чтения сроков сессий:" << query.lastError().text();
        return;
    }

    m_nextChangeUtc = query.value(0).toLongLong();
    // Место освобождается, когда expires_at_utc <= now; небольшой запас на округление.
    const qint64 maxDelayMs = 24LL * 60 * 60 * 1000;
    const qint64 delayMs = qBound<qint64>(0, (m_nextChangeUtc - now) * 1000 + 250, maxDelayMs);
    m_expiryTimer.start(static_cast<int>(delayMs));
}

bool ParkingService::loadZones(const QString &onlyZoneId, QVariantList *out) const
{
    out->clear();
    if (!m_database.isOpen()) {
        qWarning() << "ParkingService: база данных не открыта";
        return false;
    }

    QString sql = QStringLiteral(
                "SELECT z.id, z.name, z.address, z.latitude, z.longitude,"
                " z.rate_kopecks_per_hour, z.working_hours,"
                " COUNT(s.id) AS capacity,"
                " COALESCE(SUM(CASE WHEN ")
            + QLatin1String(kAvailableExpression)
            + QStringLiteral(" THEN 1 ELSE 0 END), 0) AS free_spaces"
                             " FROM zones z LEFT JOIN spaces s ON s.zone_id = z.id");
    if (!onlyZoneId.isEmpty())
        sql += QStringLiteral(" WHERE z.id = :zoneId");
    sql += QStringLiteral(" GROUP BY z.id ORDER BY z.id");

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":now"), nowUtc());
    if (!onlyZoneId.isEmpty())
        query.bindValue(QStringLiteral(":zoneId"), onlyZoneId);
    if (!query.exec()) {
        qWarning() << "ParkingService: ошибка чтения зон:" << query.lastError().text();
        return false;
    }

    while (query.next()) {
        QVariantMap zone;
        zone.insert(QStringLiteral("id"), query.value(0).toString());
        zone.insert(QStringLiteral("name"), query.value(1).toString());
        zone.insert(QStringLiteral("address"), query.value(2).toString());
        zone.insert(QStringLiteral("latitude"), query.value(3).toDouble());
        zone.insert(QStringLiteral("longitude"), query.value(4).toDouble());
        zone.insert(QStringLiteral("rateKopecksPerHour"), query.value(5).toLongLong());
        zone.insert(QStringLiteral("workingHours"), query.value(6).toString());
        zone.insert(QStringLiteral("capacity"), query.value(7).toInt());
        zone.insert(QStringLiteral("freeSpaces"), query.value(8).toInt());
        out->append(zone);
    }
    return true;
}

QVariantMap ParkingService::withDistance(const QVariantMap &zone) const
{
    QVariantMap result = zone;
    qint64 distance = -1;
    if (m_hasPosition) {
        distance = ParkingGeo::distanceMeters(m_latitude, m_longitude,
                                              zone.value(QStringLiteral("latitude")).toDouble(),
                                              zone.value(QStringLiteral("longitude")).toDouble());
    }
    result.insert(QStringLiteral("distanceMeters"), distance);
    return result;
}

void ParkingService::rebuildZones()
{
    QVariantList zones;
    zones.reserve(m_baseZones.size());
    for (const QVariant &zone : m_baseZones)
        zones.append(withDistance(zone.toMap()));
    ParkingGeo::sortZones(zones);
    m_zones = zones;
}
