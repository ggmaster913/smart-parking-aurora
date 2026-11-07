// ParkingRecommendationService (#20): правила в recommendationrules.cpp, данные — из сервисов.
#include "experience/parkingrecommendationservice.h"

#include "booking/bookingservice.h"
#include "experience/recommendationrules.h"
#include "parking/parkingservice.h"

#include <QVariantList>

namespace {

QVariantMap result(bool ok, const QString &code, const QString &message, const QVariantMap &data)
{
    QVariantMap r;
    r.insert(QStringLiteral("ok"), ok);
    r.insert(QStringLiteral("errorCode"), code);
    r.insert(QStringLiteral("message"), message);
    r.insert(QStringLiteral("data"), data);
    return r;
}

QVariantMap item(const QString &mode, const RecommendationCandidate &c, const QString &reason)
{
    QVariantMap m;
    m.insert(QStringLiteral("mode"), mode);
    m.insert(QStringLiteral("zoneId"), c.zoneId);
    m.insert(QStringLiteral("amountKopecks"), static_cast<qlonglong>(c.amountKopecks));
    m.insert(QStringLiteral("currency"), QStringLiteral("RUB"));
    m.insert(QStringLiteral("distanceMeters"), static_cast<qlonglong>(c.distanceMeters));
    m.insert(QStringLiteral("freeSpaces"), c.freeSpaces);
    m.insert(QStringLiteral("reason"), reason);
    return m;
}

} // namespace

ParkingRecommendationService::ParkingRecommendationService(ParkingService *parking,
                                                           BookingService *booking,
                                                           QObject *parent)
    : QObject(parent)
    , m_parking(parking)
    , m_booking(booking)
{
}

QVariantMap ParkingRecommendationService::recommend(int durationMinutes) const
{
    if (!BookingService::isValidDuration(durationMinutes)) {
        return result(false, QStringLiteral("INVALID_INPUT"),
                      QStringLiteral("Длительность — от 15 минут до 24 часов, шаг 15 минут."),
                      QVariantMap());
    }
    if (!m_parking || !m_booking) {
        return result(false, QStringLiteral("STORAGE_ERROR"),
                      QStringLiteral("Подбор недоступен."), QVariantMap());
    }

    // Кандидаты — только зоны со свободными местами и валидной ценой из quote.
    QVector<RecommendationCandidate> candidates;
    const QVariantList zones = m_parking->zones();
    for (const QVariant &value : zones) {
        const QVariantMap zone = value.toMap();
        const int free = zone.value(QStringLiteral("freeSpaces")).toInt();
        const QString zoneId = zone.value(QStringLiteral("id")).toString();
        if (free <= 0 || zoneId.isEmpty())
            continue;
        const QVariantMap quote = m_booking->quote(zoneId, durationMinutes);
        if (!quote.value(QStringLiteral("ok")).toBool())
            continue;
        bool amountOk = false;
        const qint64 amount = quote.value(QStringLiteral("data")).toMap()
                .value(QStringLiteral("amountKopecks")).toLongLong(&amountOk);
        if (!amountOk || amount < 0 || amount > BookingService::MaxAmountKopecks)
            continue;
        bool distanceOk = false;
        qint64 distance = zone.value(QStringLiteral("distanceMeters")).toLongLong(&distanceOk);
        if (!distanceOk || !m_parking->hasPosition())
            distance = -1;

        RecommendationCandidate c;
        c.zoneId = zoneId;
        c.amountKopecks = amount;
        c.distanceMeters = distance;
        c.freeSpaces = free;
        candidates.append(c);
    }

    QVariantList recommendations;
    const int nearest = RecommendationRules::nearest(candidates);
    if (nearest >= 0) {
        recommendations.append(item(QStringLiteral("nearest"), candidates.at(nearest),
                                    QStringLiteral("Ближе всего по прямой среди зон со свободными местами")));
    }
    const int cheapest = RecommendationRules::cheapest(candidates);
    if (cheapest >= 0) {
        recommendations.append(item(QStringLiteral("cheapest"), candidates.at(cheapest),
                                    candidates.at(cheapest).amountKopecks == 0
                                    ? QStringLiteral("Бесплатно за выбранное время")
                                    : QStringLiteral("Минимальная стоимость за выбранное время")));
    }
    bool priceOnly = false;
    const int balanced = RecommendationRules::balanced(candidates, &priceOnly);
    if (balanced >= 0) {
        recommendations.append(item(QStringLiteral("balanced"), candidates.at(balanced),
                                    priceOnly
                                    ? QStringLiteral("Геопозиция недоступна — выбрана меньшая стоимость")
                                    : QStringLiteral("Равный учёт стоимости и расстояния по прямой")));
    }

    QVariantMap data;
    data.insert(QStringLiteral("recommendations"), recommendations);
    data.insert(QStringLiteral("positionAvailable"), m_parking->hasPosition());
    return result(true, QString(), QString(), data);
}
