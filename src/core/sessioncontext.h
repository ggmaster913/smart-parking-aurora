#ifndef SESSIONCONTEXT_H
#define SESSIONCONTEXT_H
#include <QObject>
#include <QString>

class SessionContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId NOTIFY userChanged)
public:
    explicit SessionContext(QObject *parent = nullptr);
    QString userId() const;
    void setUserId(const QString &userId);
signals:
    void userChanged();
private:
    QString m_userId;
};
#endif
