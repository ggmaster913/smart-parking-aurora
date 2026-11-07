// Проверки правил подбора (#20) на общих случаях tests/experience/rule_cases.json.
// Хостовые тесты: подтверждают правила, но не сборку в Aurora SDK.
#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "experience/recommendationrules.h"

namespace {

QString zoneAt(const QVector<RecommendationCandidate> &candidates, int index)
{
    return index >= 0 ? candidates.at(index).zoneId : QString();
}

QString expected(const QJsonValue &value)
{
    return value.isNull() ? QString() : value.toString();
}

} // namespace

class TstRecommendationRules : public QObject
{
    Q_OBJECT

private slots:
    void cases_data();
    void cases();
};

void TstRecommendationRules::cases_data()
{
    QTest::addColumn<QJsonObject>("item");
    QFile file(QStringLiteral(EXPERIENCE_TEST_DIR "/rule_cases.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonArray items = QJsonDocument::fromJson(file.readAll()).object()
            .value(QStringLiteral("cases")).toArray();
    QVERIFY(items.size() >= 10);
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        QTest::newRow(qPrintable(item.value(QStringLiteral("name")).toString())) << item;
    }
}

void TstRecommendationRules::cases()
{
    QFETCH(QJsonObject, item);
    QVector<RecommendationCandidate> candidates;
    for (const QJsonValue &value : item.value(QStringLiteral("candidates")).toArray()) {
        const QJsonArray row = value.toArray();
        RecommendationCandidate c;
        c.zoneId = row.at(0).toString();
        // Числа JSON — double; целые до 2^53 − 1 представимы точно.
        c.amountKopecks = static_cast<qint64>(row.at(1).toDouble());
        c.distanceMeters = static_cast<qint64>(row.at(2).toDouble());
        c.freeSpaces = 1;
        candidates.append(c);
    }
    QCOMPARE(zoneAt(candidates, RecommendationRules::nearest(candidates)),
             expected(item.value(QStringLiteral("nearest"))));
    QCOMPARE(zoneAt(candidates, RecommendationRules::cheapest(candidates)),
             expected(item.value(QStringLiteral("cheapest"))));
    bool priceOnly = false;
    QCOMPARE(zoneAt(candidates, RecommendationRules::balanced(candidates, &priceOnly)),
             expected(item.value(QStringLiteral("balanced"))));
    QCOMPARE(priceOnly, item.value(QStringLiteral("priceOnly")).toBool());
}

QTEST_APPLESS_MAIN(TstRecommendationRules)
#include "tst_recommendationrules.moc"
