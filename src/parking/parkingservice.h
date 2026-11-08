// SPDX-License-Identifier: MIT
#ifndef PARKINGSERVICE_H
#define PARKINGSERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class SessionContext;

// ParkingService по docs/CONTRACTS.md (владелец — Илья).
// Читает таблицы zones, spaces и sessions из общей connection; ничего не пишет.
// Используется только в главном потоке.
class ParkingService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList zones READ zones NOTIFY zonesChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY positionChanged)
    Q_PROPERTY(bool positionIsDemo READ positionIsDemo NOTIFY positionChanged)

public:
    explicit ParkingService(QSqlDatabase database, SessionContext *session,
                            QObject *parent = nullptr);

    QVariantList zones() const;
    bool hasPosition() const;
    bool positionIsDemo() const;

    Q_INVOKABLE QVariantMap setPosition(double latitude, double longitude, bool isDemo);
    Q_INVOKABLE QVariantMap zoneDetails(const QString &zoneId) const;
    Q_INVOKABLE QVariantList spaces(const QString &zoneId) const;
    Q_INVOKABLE void refresh();

    // Для проверок: ближайший срок активной сессии (UTC, сек) или -1
    // и интервал запланированного обновления каталога (мс) или -1.
    qint64 nextAvailabilityChangeUtc() const;
    int scheduledRefreshMs() const;

signals:
    void zonesChanged();
    void positionChanged();

protected:
    // Текущее время UTC в секундах; виртуально для проверок истечения сессий.
    virtual qint64 nowUtc() const;

private:
    bool loadZones(const QString &onlyZoneId, QVariantList *out) const;
    bool reloadFromDatabase();
    void scheduleExpiryRefresh();
    QVariantMap withDistance(const QVariantMap &zone) const;
    void rebuildZones();

    QSqlDatabase m_database;
    SessionContext *m_session;
    QVariantList m_baseZones; // без distanceMeters, порядок из БД
    QVariantList m_zones;     // с distanceMeters, отсортировано по контракту
    bool m_hasPosition;
    bool m_positionIsDemo;
    double m_latitude;
    double m_longitude;
    // Обновление каталога в момент истечения ближайшей активной сессии,
    // чтобы список не показывал устаревшую доступность до явного refresh().
    QTimer m_expiryTimer;
    qint64 m_nextChangeUtc;
};

#endif // PARKINGSERVICE_H
