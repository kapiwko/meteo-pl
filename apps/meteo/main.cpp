#include <QCommandLineParser>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#ifdef METEO_BUNDLED_ICONS
#include <KIconTheme>
#endif

#ifdef Q_OS_ANDROID
Q_DECL_EXPORT
#endif
int main(int argc, char *argv[])
{
#ifdef METEO_BUNDLED_ICONS
    KIconTheme::initTheme();
#endif
    // KDE's native file and color dialogs require QApplication, including
    // when the application itself uses only Qt Quick controls.
    QApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/meteo.svg")));
    QCoreApplication::setApplicationName(QStringLiteral("meteo-pl"));
    QCoreApplication::setApplicationVersion(QStringLiteral(METEO_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("meteo-pl"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Meteo · meteogramy ICM"));
    QGuiApplication::setDesktopFileName(QStringLiteral("pl.meteo.desktop"));
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
#ifdef Q_OS_ANDROID
        QQuickStyle::setStyle(QStringLiteral("org.kde.breeze"));
#else
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
#endif
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meteogramy meteo.pl dla pulpitu KDE"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Sprawdź uruchomienie QML bez pobierania danych.")});
    parser.process(app);
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("smokeTest"), smokeTest}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("pl.meteo.app"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;
    if (smokeTest)
        QTimer::singleShot(1000, &app, &QCoreApplication::quit);
    return app.exec();
}
