// SPDX-License-Identifier: MIT
#include "parkinggeo.h"

#include <QtMath>
#include <QString>

#include <algorithm>

namespace {
// Средний радиус Земли (IUGG), метры.
const double kEarthRadiusMeters = 6371008.8;
} // namespace

namespace ParkingGeo {

bool isValidCoordinate(double latitude, double longitude)
{
    return qIsFinite(latitude) && qIsFinite(longitude)
            && latitude >= -90.0 && latitude <= 90.0
            && longitude >= -180.0 && longitude <= 180.0;
}

qint64 distanceMeters(double latitude1, double longitude1,
                      double latitude2, double longitude2)
{
    if (!isValidCoordinate(latitude1, longitude1)
            || !isValidCoordinate(latitude2, longitude2)) {
        return -1;
    }

    const double phi1 = qDegreesToRadians(latitude1);
    const double phi2 = qDegreesToRadians(latitude2);
    const double deltaPhi = qDegreesToRadians(latitude2 - latitude1);
    const double deltaLambda = qDegreesToRadians(longitude2 - longitude1);

    const double sinPhi = qSin(deltaPhi / 2.0);
    const double sinLambda = qSin(deltaLambda / 2.0);
    double a = sinPhi * sinPhi + qCos(phi1) * qCos(phi2) * sinLambda * sinLambda;
    a = qBound(0.0, a, 1.0);
    const double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return qRound64(kEarthRadiusMeters * c);
}

bool zoneLessThan(const QVariantMap &left, const QVariantMap &right)
{
    const qint64 leftDistance = left.value(QStringLiteral("distanceMeters"), -1).toLongLong();
    const qint64 rightDistance = right.value(QStringLiteral("distanceMeters"), -1).toLongLong();
    const bool leftKnown = leftDistance >= 0;
    const bool rightKnown = rightDistance >= 0;

    if (leftKnown != rightKnown)
        return leftKnown;
    if (leftKnown && leftDistance != rightDistance)
        return leftDistance < rightDistance;
    return left.value(QStringLiteral("id")).toString()
            < right.value(QStringLiteral("id")).toString();
}

void sortZones(QVariantList &zones)
{
    std::stable_sort(zones.begin(), zones.end(),
                     [](const QVariant &left, const QVariant &right) {
                         return zoneLessThan(left.toMap(), right.toMap());
                     });
}

} // namespace ParkingGeo
