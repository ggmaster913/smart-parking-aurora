#include <QtTest>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScopedPointer>

class StartupStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
public:
    bool ready() const { return m_ready; }
    void servicesCreated() { emit servicesReady(); }
    void recover()
    {
        m_ready = true;
        emit stateChanged(); // initialize() publishes state before services exist.
        emit retrySucceeded();
    }
signals:
    void stateChanged();
    void retrySucceeded();
    void servicesReady();
private:
    bool m_ready = false;
};

class AccountStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn CONSTANT)
public:
    bool signedIn() const { return false; }
};

class StartupRetryTest : public QObject
{
    Q_OBJECT
private slots:
    void recoveryRoutesAfterAccountRegistration()
    {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(APP_TEST_ROOT "/tests/app/retry/stubs"));
        StartupStub startup;
        AccountStub account;
        engine.rootContext()->setContextProperty(QStringLiteral("appStartup"), &startup);

        // Same connection order as main.cpp: service factory before QML connections.
        connect(&startup, &StartupStub::retrySucceeded, &engine, [&]() {
            engine.rootContext()->setContextProperty(QStringLiteral("accountService"), &account);
            startup.servicesCreated();
        });
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &errors) {
            for (const QQmlError &error : errors) {
                const QString message = error.toString();
                // Qt 5.6 requires this Connections syntax. Host Qt 5.15 deprecates it.
                if (!message.contains(QStringLiteral(
                            "Implicitly defined onFoo properties in Connections are deprecated.")))
                    warnings.append(message);
            }
        });

        QQmlComponent component(&engine, QUrl::fromLocalFile(
            QStringLiteral(APP_TEST_ROOT "/qml/SmartParking.qml")));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        QObject *stack = root->property("pageStack").value<QObject *>();
        QVERIFY(stack);
        QVERIFY(stack->property("lastUrl").toString().endsWith("StartupErrorPage.qml"));

        startup.recover();
        QVERIFY2(stack->property("lastUrl").toString().endsWith("SignInPage.qml"),
                 qPrintable(warnings.join('\n')));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
};

QTEST_MAIN(StartupRetryTest)
#include "tst_startupretry.moc"
