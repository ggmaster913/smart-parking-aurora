// SPDX-License-Identifier: MIT
#include "parkingcatalog.h"

#include "parkinggeo.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>
#include <QVariantMap>

#include <cmath>

namespace {

bool fail(QString *errorMessage, const QString &message)
{
    if (errorMessage)
        *errorMessage = message;
    return false;
}

bool readText(const QJsonObject &object, const QString &key, const QString &where,
              QString *out, QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isString() || value.toString().trimmed().isEmpty())
        return fail(errorMessage, QStringLiteral("%1: поле «%2» должно быть непустой строкой")
                    .arg(where, key));
    *out = value.toString().trimmed();
    return true;
}

bool readNumber(const QJsonObject &object, const QString &key, const QString &where,
                double *out, QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return fail(errorMessage, QStringLiteral("%1: поле «%2» должно быть числом")
                    .arg(where, key));
    *out = value.toDouble();
    return true;
}

// JSON-число без дробной части в диапазоне [0, 2^53].
bool readNonNegativeInteger(const QJsonObject &object, const QString &key,
                            const QString &where, qint64 *out, QString *errorMessage)
{
    double number = 0;
    if (!readNumber(object, key, where, &number, errorMessage))
        return false;
    const double maxExact = 9007199254740992.0; // 2^53
    if (!qIsFinite(number) || number < 0 || number > maxExact
            || std::floor(number) != number) {
        return fail(errorMessage,
                    QStringLiteral("%1: поле «%2» должно быть целым неотрицательным числом")
                    .arg(where, key));
    }
    *out = static_cast<qint64>(number);
    return true;
}

} // namespace

QVariantList ParkingCatalog::parse(const QByteArray &json, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(errorMessage, QStringLiteral("Каталог парковок повреждён: некорректный JSON"));
        return QVariantList();
    }

    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("schemaVersion")).isDouble()
            || root.value(QStringLiteral("schemaVersion")).toDouble() != 1.0) {
        fail(errorMessage, QStringLiteral("Каталог парковок: поддерживается только schemaVersion = 1"));
        return QVariantList();
    }
    if (!root.value(QStringLiteral("isDemo")).isBool()
            || !root.value(QStringLiteral("isDemo")).toBool()) {
        fail(errorMessage, QStringLiteral("Каталог парковок должен быть помечен isDemo = true"));
        return QVariantList();
    }
    const QJsonValue zonesValue = root.value(QStringLiteral("zones"));
    if (!zonesValue.isArray() || zonesValue.toArray().isEmpty()) {
        fail(errorMessage, QStringLiteral("Каталог парковок: список zones пуст или отсутствует"));
        return QVariantList();
    }

    QSet<QString> seenIds;
    QVariantList zones;
    const QJsonArray zonesArray = zonesValue.toArray();
    for (int zoneIndex = 0; zoneIndex < zonesArray.size(); ++zoneIndex) {
        const QString zoneWhere = QStringLiteral("Зона №%1").arg(zoneIndex + 1);
        if (!zonesArray.at(zoneIndex).isObject()) {
            fail(errorMessage, QStringLiteral("%1: ожидается объект").arg(zoneWhere));
            return QVariantList();
        }
        const QJsonObject zoneObject = zonesArray.at(zoneIndex).toObject();

        QString id, name, address, workingHours;
        double latitude = 0, longitude = 0;
        qint64 rate = 0;
        if (!readText(zoneObject, QStringLiteral("id"), zoneWhere, &id, errorMessage)
                || !readText(zoneObject, QStringLiteral("name"), zoneWhere, &name, errorMessage)
                || !readText(zoneObject, QStringLiteral("address"), zoneWhere, &address, errorMessage)
                || !readText(zoneObject, QStringLiteral("workingHours"), zoneWhere,
                             &workingHours, errorMessage)
                || !readNumber(zoneObject, QStringLiteral("latitude"), zoneWhere, &latitude, errorMessage)
                || !readNumber(zoneObject, QStringLiteral("longitude"), zoneWhere, &longitude, errorMessage)
                || !readNonNegativeInteger(zoneObject, QStringLiteral("rateKopecksPerHour"),
                                           zoneWhere, &rate, errorMessage)) {
            return QVariantList();
        }
        if (!ParkingGeo::isValidCoordinate(latitude, longitude)) {
            fail(errorMessage, QStringLiteral("Зона %1: недопустимые координаты").arg(id));
            return QVariantList();
        }
        if (seenIds.contains(id)) {
            fail(errorMessage, QStringLiteral("Повторяющийся идентификатор %1").arg(id));
            return QVariantList();
        }
        seenIds.insert(id);

        const QJsonValue spacesValue = zoneObject.value(QStringLiteral("spaces"));
        if (!spacesValue.isArray() || spacesValue.toArray().isEmpty()) {
            fail(errorMessage, QStringLiteral("Зона %1: список мест пуст или отсутствует").arg(id));
            return QVariantList();
        }

        QSet<QString> labels;
        QVariantList spaces;
        const QJsonArray spacesArray = spacesValue.toArray();
        for (int spaceIndex = 0; spaceIndex < spacesArray.size(); ++spaceIndex) {
            const QString spaceWhere = QStringLiteral("Зона %1, место №%2").arg(id).arg(spaceIndex + 1);
            if (!spacesArray.at(spaceIndex).isObject()) {
                fail(errorMessage, QStringLiteral("%1: ожидается объект").arg(spaceWhere));
                return QVariantList();
            }
            const QJsonObject spaceObject = spacesArray.at(spaceIndex).toObject();
            QString spaceId, label;
            if (!readText(spaceObject, QStringLiteral("id"), spaceWhere, &spaceId, errorMessage)
                    || !readText(spaceObject, QStringLiteral("label"), spaceWhere, &label, errorMessage)) {
                return QVariantList();
            }
            if (!spaceObject.value(QStringLiteral("occupied")).isBool()) {
                fail(errorMessage, QStringLiteral("%1: поле «occupied» должно быть true/false")
                     .arg(spaceWhere));
                return QVariantList();
            }
            if (seenIds.contains(spaceId)) {
                fail(errorMessage, QStringLiteral("Повторяющийся идентификатор %1").arg(spaceId));
                return QVariantList();
            }
            if (labels.contains(label)) {
                fail(errorMessage, QStringLiteral("Зона %1: повторяющийся номер места %2").arg(id, label));
                return QVariantList();
            }
            seenIds.insert(spaceId);
            labels.insert(label);

            QVariantMap space;
            space.insert(QStringLiteral("id"), spaceId);
            space.insert(QStringLiteral("label"), label);
            space.insert(QStringLiteral("occupied"), spaceObject.value(QStringLiteral("occupied")).toBool());
            spaces.append(space);
        }

        QVariantMap zone;
        zone.insert(QStringLiteral("id"), id);
        zone.insert(QStringLiteral("name"), name);
        zone.insert(QStringLiteral("address"), address);
        zone.insert(QStringLiteral("latitude"), latitude);
        zone.insert(QStringLiteral("longitude"), longitude);
        zone.insert(QStringLiteral("rateKopecksPerHour"), rate);
        zone.insert(QStringLiteral("workingHours"), workingHours);
        zone.insert(QStringLiteral("spaces"), spaces);
        zones.append(zone);
    }

    if (errorMessage)
        errorMessage->clear();
    return zones;
}

QVariantList ParkingCatalog::loadFile(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(errorMessage, QStringLiteral("Не удалось открыть каталог парковок: %1").arg(path));
        return QVariantList();
    }
    return parse(file.readAll(), errorMessage);
}
