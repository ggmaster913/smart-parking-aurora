// SPDX-License-Identifier: MIT
#ifndef PARKINGCATALOG_H
#define PARKINGCATALOG_H

#include <QByteArray>
#include <QString>
#include <QVariantList>

// Разбор и полная проверка data/parking-zones.json (формат из docs/CONTRACTS.md).
// Предназначен для первичного импорта интегратором: при любой ошибке
// возвращается пустой список и русское описание, частичных данных нет.
//
// Каждая зона результата: id, name, address, latitude, longitude,
// rateKopecksPerHour (qint64), workingHours, spaces[]{id, label, occupied}.
class ParkingCatalog
{
public:
    static QVariantList parse(const QByteArray &json, QString *errorMessage);
    static QVariantList loadFile(const QString &path, QString *errorMessage);
};

#endif // PARKINGCATALOG_H
