// Тестовая заглушка SessionContext по docs/CONTRACTS.md.
// Используется только хостовыми тестами tests/booking, пока настоящий
// src/core/sessioncontext.h из #2 не появился в ветке. В приложение не входит.
#ifndef SESSIONCONTEXT_H
#define SESSIONCONTEXT_H

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
