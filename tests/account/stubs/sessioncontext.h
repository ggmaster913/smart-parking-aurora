// SPDX-License-Identifier: MIT
#ifndef SESSIONCONTEXT_H
#define SESSIONCONTEXT_H

// Тестовая заглушка ТОЛЬКО для хостовых проверок tests/account.
// Повторяет сигнатуру SessionContext из docs/CONTRACTS.md; в приложение не входит.
// Настоящий класс — src/core/sessioncontext.h (владелец — интегратор, #2).

#include <QObject>
#include <QString>

class SessionContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId NOTIFY userChanged)

public:
    explicit SessionContext(QObject *parent = nullptr) : QObject(parent) {}

    QString userId() const { return m_userId; }

    void setUserId(const QString &userId)
    {
        if (m_userId == userId)
            return;
        m_userId = userId;
        emit userChanged();
    }

signals:
    void userChanged();

private:
    QString m_userId;
};

#endif // SESSIONCONTEXT_H
