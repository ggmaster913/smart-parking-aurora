// SPDX-License-Identifier: MIT
#ifndef PARKINGGEO_H
#define PARKINGGEO_H

#include <QtGlobal>
#include <QVariantList>
#include <QVariantMap>

// Чистые функции геометрии и сортировки каталога. Без БД и QML,
// поэтому проверяются отдельно (tests/parking).
namespace ParkingGeo {

// Широта и долгота конечны и лежат в [-90, 90] и [-180, 180].
bool isValidCoordinate(double latitude, double longitude);

// Расстояние по прямой (по дуге большого круга) в метрах, округлённое
// до целого. Это не длина пешего или автомобильного маршрута.
// Для невалидных координат возвращает -1.
qint64 distanceMeters(double latitude1, double longitude1,
                      double latitude2, double longitude2);

// Порядок зон по контракту: известное расстояние по возрастанию, затем ID.
// Зоны без расстояния (distanceMeters = -1) идут после, между собой по ID.
bool zoneLessThan(const QVariantMap &left, const QVariantMap &right);
void sortZones(QVariantList &zones);

} // namespace ParkingGeo

#endif // PARKINGGEO_H
