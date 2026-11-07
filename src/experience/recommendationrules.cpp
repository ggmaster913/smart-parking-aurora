// Правила подбора (#20). Сравнения детерминированы: при равенстве решает ID зоны.
#include "experience/recommendationrules.h"

namespace {

bool hasDistance(const RecommendationCandidate &c) { return c.distanceMeters >= 0; }

// Ближе: расстояние, затем цена, затем ID.
bool nearerThan(const RecommendationCandidate &a, const RecommendationCandidate &b)
{
    if (a.distanceMeters != b.distanceMeters)
        return a.distanceMeters < b.distanceMeters;
    if (a.amountKopecks != b.amountKopecks)
        return a.amountKopecks < b.amountKopecks;
    return a.zoneId < b.zoneId;
}

// Дешевле: цена, затем известное расстояние (неизвестное в конце), затем ID.
bool cheaperThan(const RecommendationCandidate &a, const RecommendationCandidate &b)
{
    if (a.amountKopecks != b.amountKopecks)
        return a.amountKopecks < b.amountKopecks;
    if (hasDistance(a) != hasDistance(b))
        return hasDistance(a);
    if (hasDistance(a) && a.distanceMeters != b.distanceMeters)
        return a.distanceMeters < b.distanceMeters;
    return a.zoneId < b.zoneId;
}

} // namespace

namespace RecommendationRules {

int nearest(const QVector<RecommendationCandidate> &candidates)
{
    int best = -1;
    for (int i = 0; i < candidates.size(); ++i) {
        if (!hasDistance(candidates.at(i)))
            continue;
        if (best < 0 || nearerThan(candidates.at(i), candidates.at(best)))
            best = i;
    }
    return best;
}

int cheapest(const QVector<RecommendationCandidate> &candidates)
{
    int best = -1;
    for (int i = 0; i < candidates.size(); ++i) {
        if (best < 0 || cheaperThan(candidates.at(i), candidates.at(best)))
            best = i;
    }
    return best;
}

int balanced(const QVector<RecommendationCandidate> &candidates, bool *usedPriceOnly)
{
    if (usedPriceOnly)
        *usedPriceOnly = false;

    qint64 minPrice = 0, maxPrice = 0, minDistance = 0, maxDistance = 0;
    bool any = false;
    for (const RecommendationCandidate &c : candidates) {
        if (!hasDistance(c))
            continue;
        if (!any) {
            minPrice = maxPrice = c.amountKopecks;
            minDistance = maxDistance = c.distanceMeters;
            any = true;
            continue;
        }
        minPrice = qMin(minPrice, c.amountKopecks);
        maxPrice = qMax(maxPrice, c.amountKopecks);
        minDistance = qMin(minDistance, c.distanceMeters);
        maxDistance = qMax(maxDistance, c.distanceMeters);
    }
    if (!any) {
        if (usedPriceOnly)
            *usedPriceOnly = true;
        return cheapest(candidates);
    }

    const double priceRange = static_cast<double>(maxPrice - minPrice);
    const double distanceRange = static_cast<double>(maxDistance - minDistance);
    int best = -1;
    double bestScore = 0.0;
    for (int i = 0; i < candidates.size(); ++i) {
        const RecommendationCandidate &c = candidates.at(i);
        if (!hasDistance(c))
            continue;
        const double priceShare = priceRange > 0.0
                ? static_cast<double>(c.amountKopecks - minPrice) / priceRange : 0.0;
        const double distanceShare = distanceRange > 0.0
                ? static_cast<double>(c.distanceMeters - minDistance) / distanceRange : 0.0;
        const double score = 0.5 * priceShare + 0.5 * distanceShare;
        if (best < 0 || score < bestScore
                || (score == bestScore && c.zoneId < candidates.at(best).zoneId)) {
            best = i;
            bestScore = score;
        }
    }
    return best;
}

} // namespace RecommendationRules
