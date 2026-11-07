#include "sessioncontext.h"
SessionContext::SessionContext(QObject *parent) : QObject(parent) {}
QString SessionContext::userId() const { return m_userId; }
void SessionContext::setUserId(const QString &userId) {
    if (m_userId == userId) return;
    m_userId = userId;
    emit userChanged();
}
