// BookingService — бизнес-логика бронирования (#9), контракт docs/CONTRACTS.md.
// Деньги — целые копейки RUB, время — Unix UTC в секундах.
// Оплата демонстрационная: банковские данные не принимаются и не хранятся.
#ifndef BOOKINGSERVICE_H
#define BOOKINGSERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class SessionContext;

class BookingService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap activeSession READ activeSession NOTIFY activeSessionChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    BookingService(QSqlDatabase db, SessionContext *context, QObject *parent = nullptr);

    QVariantMap activeSession() const;
    QVariantList history() const;

    Q_INVOKABLE QVariantMap quote(const QString &zoneId, int durationMinutes) const;
    Q_INVOKABLE QVariantMap book(const QString &zoneId, const QString &spaceId,
                                 const QString &vehicleId, int durationMinutes,
                                 const QString &operationId);
    Q_INVOKABLE QVariantMap extend(const QString &sessionId, int additionalMinutes,
                                   const QString &operationId);
    // Досрочное завершение своей активной сессии: срок = текущий момент, место
    // освобождается, оплаченная демо-сумма не меняется. Повтор для уже завершённой
    // своей сессии возвращает её сохранённое состояние (естественная идемпотентность,
    // без записи в payment_operations — схема не меняется).
    Q_INVOKABLE QVariantMap finish(const QString &sessionId);
    Q_INVOKABLE void refresh();

    // Чистые правила, доступные тестам и другим C++-модулям.
    static const int MinDurationMinutes = 15;
    static const int MaxDurationMinutes = 1440;
    static const int DurationStepMinutes = 15;
    // Предел суммы: 2^53 − 1 коп. — целые точно представимы в JSON (response_json)
    // и в числе QML, поэтому повтор операции возвращает ту же сумму.
    static const qint64 MaxAmountKopecks = 9007199254740991LL;
    static bool isValidDuration(int minutes);
    // ceil(rate * minutes / 60) с 64-битным промежуточным произведением.
    // Возвращает -1 для отрицательного тарифа, переполнения или суммы выше MaxAmountKopecks.
    static qint64 priceKopecks(qint64 rateKopecksPerHour, int minutes);

signals:
    void activeSessionChanged();
    void historyChanged();

protected:
    // Текущее время UTC в секундах. Переопределяется только в тестах.
    virtual qint64 currentUtc() const;

private:
    QVariantMap loadActiveSession(const QString &userId) const;
    bool completeExpired(qint64 nowUtc);
    void setActiveSession(const QVariantMap &session);

    QSqlDatabase m_db;
    SessionContext *m_context;
    QVariantMap m_activeSession;
    QVariantList m_history;
};

#endif // BOOKINGSERVICE_H
