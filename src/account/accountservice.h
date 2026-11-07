// SPDX-License-Identifier: MIT
#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class SessionContext;

// AccountService по docs/CONTRACTS.md (владелец — Матвей, #3).
// Демо-вход, профиль и автомобили. Пишет только таблицы users, vehicles и settings
// (ключ active_user_id). Используется только в главном потоке.
class AccountService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QVariantList vehicles READ vehicles NOTIFY vehiclesChanged)

public:
    explicit AccountService(QSqlDatabase database, SessionContext *session,
                            QObject *parent = nullptr);

    bool signedIn() const;
    QVariantMap profile() const;
    QVariantList vehicles() const;

    Q_INVOKABLE QVariantMap demoSignIn(const QString &contact, const QString &code);
    Q_INVOKABLE QVariantMap saveProfile(const QString &displayName);
    Q_INVOKABLE QVariantMap addVehicle(const QString &plate, const QString &brand);
    Q_INVOKABLE void signOut();
    // Восстанавливает активного пользователя из settings.active_user_id.
    Q_INVOKABLE void refresh();

signals:
    void signedInChanged();
    void profileChanged();
    void vehiclesChanged();

private:
    bool loadUser(const QString &userId, QVariantMap *profile, QVariantList *vehicles) const;
    void applyState(const QString &userId, const QVariantMap &profile,
                    const QVariantList &vehicles);
    void applySignedOut();

    QSqlDatabase m_database;
    SessionContext *m_session;
    QString m_userId;
    QVariantMap m_profile;
    QVariantList m_vehicles;
};

#endif // ACCOUNTSERVICE_H
