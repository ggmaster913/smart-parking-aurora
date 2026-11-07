// Чистые правила подбора «Ближе / Дешевле / Баланс» (#20). Без БД и сервисов.
// Цена — конечная стоимость выбранной длительности из BookingService::quote, целые копейки.
// Расстояние — по прямой в метрах, -1 означает «позиция неизвестна».
#ifndef RECOMMENDATIONRULES_H
#define RECOMMENDATIONRULES_H

#include <QString>
#include <QVector>

struct RecommendationCandidate
{
    QString zoneId;
    qint64 amountKopecks = 0;
    qint64 distanceMeters = -1;
    int freeSpaces = 0;
};

namespace RecommendationRules {

// Индекс выбранного кандидата или -1, если выбрать нельзя.
// Ближе: минимальное известное расстояние, затем цена, затем ID. Без позиции — -1.
int nearest(const QVector<RecommendationCandidate> &candidates);
// Дешевле: минимальная цена, затем известное расстояние (неизвестное в конце), затем ID.
int cheapest(const QVector<RecommendationCandidate> &candidates);
// Баланс: равные веса нормализованных цены и расстояния среди кандидатов с позицией;
// при одинаковом диапазоне соответствующая доля 0; равенство — по ID.
// Если ни у кого нет позиции, выбирается cheapest() и *usedPriceOnly = true.
int balanced(const QVector<RecommendationCandidate> &candidates, bool *usedPriceOnly);

} // namespace RecommendationRules

#endif // RECOMMENDATIONRULES_H
