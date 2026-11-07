// SPDX-License-Identifier: MIT
#include "accountservice.h"

#include "accountvalidation.h"
#include "sessioncontext.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <QtDebug>

namespace {

const char kActiveUserKey[] = "active_user_id";

QVariantMap okResult(const QVariantMap &data = QVariantMap())
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("errorCode"), QString());
    result.insert(QStringLiteral("message"), QString());
    result.insert(QStringLiteral("data"), data);
    return result;
}

QVariantMap errorResult(const QString &errorCode, const QString &message)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("errorCode"), errorCode);
    result.insert(QStringLiteral("message"), message);
    result.insert(QStringLiteral("data"), QVariantMap());
    return result;
}

QVariantMap invalidInput(const QString &message)
{
    return errorResult(QStringLiteral("INVALID_INPUT"), message);
}

QVariantMap notAuthenticated()
{
    return errorResult(QStringLiteral("NOT_AUTHENTICATED"),
                       QStringLiteral("Сначала выполните демо-вход."));
}

QVariantMap storageError()
{
    return errorResult(QStringLiteral("STORAGE_ERROR"),
                       QStringLiteral("Не удалось сохранить данные на устройстве. Повторите попытку."));
}

QString newId()
{
    // Формат без фигурных скобок; QUuid::WithoutBraces недоступен в Qt 5.6.
    return QUuid::createUuid().toString().mid(1, 36);
}

void logQueryError(const char *where, const QSqlQuery &query)
{
    qWarning() << "AccountService:" << where << query.lastError().text();
}

QVariantMap vehicleMap(const QString &id, const QString &plate, const QString &brand)
{
    QVariantMap vehicle;
    vehicle.insert(QStringLiteral("id"), id);
    vehicle.insert(QStringLiteral("plate"), plate);
    vehicle.insert(QStringLiteral("brand"), brand);
    return vehicle;
}

// Откатывает открытую транзакцию, если её не зафиксировали явно.
class TransactionGuard
{
public:
    explicit TransactionGuard(QSqlDatabase database)
        : m_database(database), m_active(database.transaction())
    {
    }
    ~TransactionGuard()
    {
        if (m_active)
            m_database.rollback();
    }
    bool started() const { return m_active; }
    bool commit()
    {
        if (!m_active)
            return false;
        m_active = !m_database.commit();
        return !m_active;
    }

private:
    QSqlDatabase m_database;
    bool m_active;
};

} // namespace

AccountService::AccountService(QSqlDatabase database, SessionContext *session, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_session(session)
{
    refresh();
}

bool AccountService::signedIn() const
{
    return !m_userId.isEmpty();
}

QVariantMap AccountService::profile() const
{
    return m_profile;
}

QVariantList AccountService::vehicles() const
{
    return m_vehicles;
}

QVariantMap AccountService::demoSignIn(const QString &contact, const QString &code)
{
    QString normalizedContact;
    if (!AccountValidation::normalizeContact(contact, &normalizedContact)) {
        return invalidInput(QStringLiteral(
                "Введите email (например, name@example.ru) или телефон из 10–15 цифр."));
    }
    if (!AccountValidation::isDemoCodeValid(code)) {
        return invalidInput(QStringLiteral("Неверный демо-код. Для демо-входа используйте %1.")
                                    .arg(AccountValidation::demoCode()));
    }

    TransactionGuard transaction(m_database);
    if (!transaction.started())
        return storageError();

    QString userId;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT id FROM users WHERE contact = ?"));
    query.addBindValue(normalizedContact);
    if (!query.exec()) {
        logQueryError("select user", query);
        return storageError();
    }
    if (query.next())
        userId = query.value(0).toString();
    query.finish();

    if (userId.isEmpty()) {
        userId = newId();
        QSqlQuery insert(m_database);
        insert.prepare(QStringLiteral(
                "INSERT INTO users (id, contact, display_name) VALUES (?, ?, '')"));
        insert.addBindValue(userId);
        insert.addBindValue(normalizedContact);
        if (!insert.exec()) {
            logQueryError("insert user", insert);
            return storageError();
        }
    }

    QSqlQuery setting(m_database);
    setting.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    setting.addBindValue(QString::fromLatin1(kActiveUserKey));
    setting.addBindValue(userId);
    if (!setting.exec()) {
        logQueryError("save active user", setting);
        return storageError();
    }

    QVariantMap loadedProfile;
    QVariantList loadedVehicles;
    if (!loadUser(userId, &loadedProfile, &loadedVehicles) || !transaction.commit())
        return storageError();

    applyState(userId, loadedProfile, loadedVehicles);

    QVariantMap data;
    data.insert(QStringLiteral("profile"), m_profile);
    return okResult(data);
}

QVariantMap AccountService::saveProfile(const QString &displayName)
{
    if (!signedIn())
        return notAuthenticated();

    QString normalizedName;
    if (!AccountValidation::normalizeDisplayName(displayName, &normalizedName))
        return invalidInput(QStringLiteral("Введите имя длиной от 1 до 100 символов."));

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE users SET display_name = ? WHERE id = ?"));
    query.addBindValue(normalizedName);
    query.addBindValue(m_userId);
    if (!query.exec()) {
        logQueryError("update profile", query);
        return storageError();
    }
    if (query.numRowsAffected() != 1) {
        applySignedOut();
        return errorResult(QStringLiteral("NOT_FOUND"),
                           QStringLiteral("Профиль не найден. Выполните вход заново."));
    }

    m_profile.insert(QStringLiteral("displayName"), normalizedName);
    emit profileChanged();

    QVariantMap data;
    data.insert(QStringLiteral("profile"), m_profile);
    return okResult(data);
}

QVariantMap AccountService::addVehicle(const QString &plate, const QString &brand)
{
    if (!signedIn())
        return notAuthenticated();

    QString normalizedPlate;
    if (!AccountValidation::normalizePlate(plate, &normalizedPlate))
        return invalidInput(QStringLiteral("Госномер должен содержать от 4 до 15 символов."));

    QString normalizedBrand;
    if (!AccountValidation::normalizeBrand(brand, &normalizedBrand))
        return invalidInput(QStringLiteral("Марка должна содержать от 1 до 60 символов."));

    const QVariantMap duplicate = invalidInput(
            QStringLiteral("Автомобиль с номером %1 уже добавлен.").arg(normalizedPlate));

    QSqlQuery exists(m_database);
    exists.prepare(QStringLiteral("SELECT 1 FROM vehicles WHERE user_id = ? AND plate = ?"));
    exists.addBindValue(m_userId);
    exists.addBindValue(normalizedPlate);
    if (!exists.exec()) {
        logQueryError("check vehicle", exists);
        return storageError();
    }
    if (exists.next())
        return duplicate;
    exists.finish();

    const QString vehicleId = newId();
    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
            "INSERT INTO vehicles (id, user_id, plate, brand) VALUES (?, ?, ?, ?)"));
    insert.addBindValue(vehicleId);
    insert.addBindValue(m_userId);
    insert.addBindValue(normalizedPlate);
    insert.addBindValue(normalizedBrand);
    if (!insert.exec()) {
        logQueryError("insert vehicle", insert);
        return storageError();
    }

    const QVariantMap vehicle = vehicleMap(vehicleId, normalizedPlate, normalizedBrand);
    m_vehicles.append(vehicle);
    emit vehiclesChanged();

    QVariantMap data;
    data.insert(QStringLiteral("vehicle"), vehicle);
    return okResult(data);
}

void AccountService::signOut()
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = ?"));
    query.addBindValue(QString::fromLatin1(kActiveUserKey));
    if (!query.exec()) {
        logQueryError("clear active user", query);
        // Выход не подтверждён: settings.active_user_id остался, и после перезапуска
        // вход восстановится. Профиль и SessionContext не трогаем, сообщаем об ошибке.
        const QVariantMap error = storageError();
        emit operationFailed(error.value(QStringLiteral("errorCode")).toString(),
                             error.value(QStringLiteral("message")).toString());
        return;
    }

    // История и записи пользователя остаются в БД, очищается только видимое состояние.
    applySignedOut();
}

void AccountService::refresh()
{
    if (!m_database.isOpen()) {
        applySignedOut();
        return;
    }

    QString userId;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(QString::fromLatin1(kActiveUserKey));
    if (!query.exec()) {
        logQueryError("read active user", query);
        applySignedOut();
        return;
    }
    if (query.next())
        userId = query.value(0).toString();
    query.finish();

    QVariantMap loadedProfile;
    QVariantList loadedVehicles;
    if (userId.isEmpty() || !loadUser(userId, &loadedProfile, &loadedVehicles)) {
        applySignedOut();
        return;
    }
    applyState(userId, loadedProfile, loadedVehicles);
}

bool AccountService::loadUser(const QString &userId, QVariantMap *loadedProfile,
                              QVariantList *loadedVehicles) const
{
    QSqlQuery user(m_database);
    user.prepare(QStringLiteral("SELECT id, contact, display_name FROM users WHERE id = ?"));
    user.addBindValue(userId);
    if (!user.exec()) {
        logQueryError("load user", user);
        return false;
    }
    if (!user.next())
        return false;

    QVariantMap profile;
    profile.insert(QStringLiteral("id"), user.value(0).toString());
    profile.insert(QStringLiteral("contact"), user.value(1).toString());
    profile.insert(QStringLiteral("displayName"), user.value(2).toString());
    user.finish();

    QSqlQuery cars(m_database);
    cars.prepare(QStringLiteral(
            "SELECT id, plate, brand FROM vehicles WHERE user_id = ? ORDER BY rowid"));
    cars.addBindValue(userId);
    if (!cars.exec()) {
        logQueryError("load vehicles", cars);
        return false;
    }
    QVariantList vehicles;
    while (cars.next()) {
        vehicles.append(vehicleMap(cars.value(0).toString(), cars.value(1).toString(),
                                   cars.value(2).toString()));
    }

    *loadedProfile = profile;
    *loadedVehicles = vehicles;
    return true;
}

void AccountService::applyState(const QString &userId, const QVariantMap &profile,
                                const QVariantList &vehicles)
{
    const bool wasSignedIn = signedIn();
    const bool userChanged = m_userId != userId;
    const bool profileDiffers = m_profile != profile;
    const bool vehiclesDiffer = m_vehicles != vehicles;

    m_userId = userId;
    m_profile = profile;
    m_vehicles = vehicles;

    if (m_session && userChanged)
        m_session->setUserId(userId);
    if (!wasSignedIn)
        emit signedInChanged();
    if (profileDiffers)
        emit profileChanged();
    if (vehiclesDiffer)
        emit vehiclesChanged();
}

void AccountService::applySignedOut()
{
    const bool wasSignedIn = signedIn();
    const bool hadProfile = !m_profile.isEmpty();
    const bool hadVehicles = !m_vehicles.isEmpty();

    m_userId.clear();
    m_profile.clear();
    m_vehicles.clear();

    if (m_session && wasSignedIn)
        m_session->setUserId(QString());
    if (wasSignedIn)
        emit signedInChanged();
    if (hadProfile)
        emit profileChanged();
    if (hadVehicles)
        emit vehiclesChanged();
}
