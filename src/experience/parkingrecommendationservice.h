// ParkingRecommendationService — подбор «Ближе / Дешевле / Баланс» (#20).
// Считает по текущим данным при каждом вызове: свободные зоны и расстояние — ParkingService,
// конечная цена выбранной длительности — BookingService::quote. Ничего не хранит и не бронирует.
#ifndef PARKINGRECOMMENDATIONSERVICE_H
#define PARKINGRECOMMENDATIONSERVICE_H

#include <QObject>
#include <QVariantMap>

class BookingService;
class ParkingService;

class ParkingRecommendationService : public QObject
{
    Q_OBJECT

public:
    ParkingRecommendationService(ParkingService *parking, BookingService *booking,
                                 QObject *parent = nullptr);

    // Result: data.recommendations — список {mode: nearest|cheapest|balanced, zoneId,
    // amountKopecks, currency: "RUB", distanceMeters, freeSpaces, reason};
    // data.positionAvailable — bool. Неверная длительность — INVALID_INPUT.
    // Пустой или полностью занятый каталог — успешный пустой список.
    Q_INVOKABLE QVariantMap recommend(int durationMinutes) const;

private:
    ParkingService *m_parking;
    BookingService *m_booking;
};

#endif // PARKINGRECOMMENDATIONSERVICE_H
