// SPDX-License-Identifier: MIT
// Корень приложения SmartParking (#28): общая SQLite через DatabaseManager, однократный
// импорт демо-каталога и регистрация сервисов для QML. Одна connection, главный поток.
#include <auroraapp.h>
#include <QtQuick>

#include <QScopedPointer>
#include <QtDebug>

#include "account/accountservice.h"
#include "booking/bookingservice.h"
#include "core/catalogseed.h"
#include "core/databasemanager.h"
#include "core/sessioncontext.h"
#include "history/historyservice.h"
#include "parking/parkingservice.h"

// Состояние запуска для QML: ошибка открытия БД или импорта показывается на отдельном
// экране с повтором, без скрытой временной БД.
class AppStartup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

public:
    AppStartup(DatabaseManager *database, const QString &catalogPath, QObject *parent = nullptr)
        : QObject(parent), m_database(database), m_catalogPath(catalogPath)
    {
    }

    bool ready() const { return m_ready; }
    QString errorText() const { return m_errorText; }

    void servicesCreated() { emit servicesReady(); }

    bool initialize()
    {
        QString error;
        if (!m_database->open())
            error = QStringLiteral("Не удалось открыть базу данных: %1").arg(m_database->lastError());
        else
            CatalogSeed::ensureSeeded(m_database->connection(), m_catalogPath, &error);
        m_ready = error.isEmpty();
        m_errorText = error;
        if (!m_ready)
            qWarning() << "SmartParking: запуск не выполнен:" << error;
        emit stateChanged();
        return m_ready;
    }

    Q_INVOKABLE void retry()
    {
        if (!m_ready && initialize())
            emit retrySucceeded();
    }

signals:
    void stateChanged();
    void retrySucceeded();
    void servicesReady();

private:
    DatabaseManager *m_database;
    QString m_catalogPath;
    bool m_ready = false;
    QString m_errorText;
};

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.iiexperts"));
    application->setApplicationName(QStringLiteral("SmartParking"));

    // Порядок объявлений задаёт порядок уничтожения: view -> сервисы -> контекст -> менеджер БД.
    DatabaseManager database;
    const QString catalogPath =
            Aurora::Application::pathTo(QStringLiteral("data/parking-zones.json")).toLocalFile();
    AppStartup startup(&database, catalogPath);
    startup.initialize();

    SessionContext session;
    QScopedPointer<AccountService> accountService;
    QScopedPointer<ParkingService> parkingService;
    QScopedPointer<BookingService> bookingService;
    QScopedPointer<HistoryService> historyService;
    QScopedPointer<QQuickView> view(Aurora::Application::createView());

    auto createServices = [&]() {
        if (!accountService.isNull())
            return;
        const QSqlDatabase connection = database.connection();
        accountService.reset(new AccountService(connection, &session));
        parkingService.reset(new ParkingService(connection, &session));
        bookingService.reset(new BookingService(connection, &session));
        historyService.reset(new HistoryService(connection, &session));

        // Новая, продлённая или завершённая сессия меняет доступность мест и историю.
        HistoryService *history = historyService.data();
        QObject::connect(bookingService.data(), &BookingService::activeSessionChanged,
                         parkingService.data(), &ParkingService::refresh);
        QObject::connect(bookingService.data(), &BookingService::activeSessionChanged,
                         history, [history]() { history->refresh(); });

        bookingService->refresh(); // завершает истёкшие сессии
        parkingService->refresh();
        history->refresh();

        QQmlContext *context = view->rootContext();
        context->setContextProperty(QStringLiteral("accountService"), accountService.data());
        context->setContextProperty(QStringLiteral("parkingService"), parkingService.data());
        context->setContextProperty(QStringLiteral("bookingService"), bookingService.data());
        context->setContextProperty(QStringLiteral("historyService"), historyService.data());
        startup.servicesCreated();
    };

    if (startup.ready())
        createServices();
    QObject::connect(&startup, &AppStartup::retrySucceeded, createServices);

    view->engine()->addImportPath(
            Aurora::Application::pathTo(QStringLiteral("lib/qmlplugins/")).toString());
    view->rootContext()->setContextProperty(QStringLiteral("appStartup"), &startup);
    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/SmartParking.qml")));
    view->show();

    const int result = application->exec();
    view.reset(); // QML уничтожается раньше сервисов и БД
    return result;
}

#include "main.moc"
