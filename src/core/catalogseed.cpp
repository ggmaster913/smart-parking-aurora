// SPDX-License-Identifier: MIT
#include "catalogseed.h"

#include "parking/parkingcatalog.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVariantMap>

namespace CatalogSeed {

const char kVersionKey[] = "catalog_seed_version";
const char kVersion[] = "1";

namespace {

bool fail(QSqlDatabase &database, bool inTransaction, const QString &message,
          QString *errorMessage)
{
    if (inTransaction)
        database.rollback();
    if (errorMessage)
        *errorMessage = message;
    return false;
}

QString queryError(const QString &where, const QSqlQuery &query)
{
    return QStringLiteral("%1: %2").arg(where, query.lastError().text());
}

} // namespace

bool ensureSeeded(QSqlDatabase database, const QString &jsonPath, QString *errorMessage)
{
    if (!database.isOpen())
        return fail(database, false, QStringLiteral("База данных не открыта"), errorMessage);

    QSqlQuery marker(database);
    marker.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    marker.addBindValue(QString::fromLatin1(kVersionKey));
    if (!marker.exec())
        return fail(database, false, queryError(QStringLiteral("Чтение маркера каталога"), marker),
                    errorMessage);
    if (marker.next())
        return true; // Уже импортирован: повторный запуск ничего не пересевает.
    marker.finish();

    QString parseError;
    const QVariantList zones = ParkingCatalog::loadFile(jsonPath, &parseError);
    if (zones.isEmpty())
        return fail(database, false,
                    QStringLiteral("Каталог парковок не загружен: %1").arg(parseError),
                    errorMessage);

    if (!database.transaction())
        return fail(database, false,
                    QStringLiteral("Не удалось начать транзакцию: %1")
                            .arg(database.lastError().text()),
                    errorMessage);

    QSqlQuery zoneInsert(database);
    zoneInsert.prepare(QStringLiteral(
            "INSERT INTO zones (id, name, address, latitude, longitude, rate_kopecks_per_hour,"
            " working_hours) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery spaceInsert(database);
    spaceInsert.prepare(QStringLiteral(
            "INSERT INTO spaces (id, zone_id, label, mock_occupied) VALUES (?, ?, ?, ?)"));

    for (const QVariant &zoneValue : zones) {
        const QVariantMap zone = zoneValue.toMap();
        zoneInsert.addBindValue(zone.value(QStringLiteral("id")).toString());
        zoneInsert.addBindValue(zone.value(QStringLiteral("name")).toString());
        zoneInsert.addBindValue(zone.value(QStringLiteral("address")).toString());
        zoneInsert.addBindValue(zone.value(QStringLiteral("latitude")).toDouble());
        zoneInsert.addBindValue(zone.value(QStringLiteral("longitude")).toDouble());
        zoneInsert.addBindValue(zone.value(QStringLiteral("rateKopecksPerHour")).toLongLong());
        zoneInsert.addBindValue(zone.value(QStringLiteral("workingHours")).toString());
        if (!zoneInsert.exec())
            return fail(database, true, queryError(QStringLiteral("Импорт зоны"), zoneInsert),
                        errorMessage);

        const QVariantList spaces = zone.value(QStringLiteral("spaces")).toList();
        for (const QVariant &spaceValue : spaces) {
            const QVariantMap space = spaceValue.toMap();
            spaceInsert.addBindValue(space.value(QStringLiteral("id")).toString());
            spaceInsert.addBindValue(zone.value(QStringLiteral("id")).toString());
            spaceInsert.addBindValue(space.value(QStringLiteral("label")).toString());
            spaceInsert.addBindValue(space.value(QStringLiteral("occupied")).toBool() ? 1 : 0);
            if (!spaceInsert.exec())
                return fail(database, true, queryError(QStringLiteral("Импорт места"), spaceInsert),
                            errorMessage);
        }
    }

    QSqlQuery markerInsert(database);
    markerInsert.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?)"));
    markerInsert.addBindValue(QString::fromLatin1(kVersionKey));
    markerInsert.addBindValue(QString::fromLatin1(kVersion));
    if (!markerInsert.exec())
        return fail(database, true, queryError(QStringLiteral("Запись маркера каталога"), markerInsert),
                    errorMessage);

    if (!database.commit())
        return fail(database, true,
                    QStringLiteral("Не удалось сохранить каталог: %1")
                            .arg(database.lastError().text()),
                    errorMessage);
    return true;
}

} // namespace CatalogSeed
