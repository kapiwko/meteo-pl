#include "core/Localization.hpp"
#include "core/Forecast.hpp"
#include <KLocalizedString>
#include <QCommandLineParser>
#include <QDebug>
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
    KLocalizedString::setApplicationDomain("meteo-pl");
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/meteo.svg")));
    QCoreApplication::setApplicationName(QStringLiteral("meteo-pl"));
    QCoreApplication::setApplicationVersion(QStringLiteral(METEO_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("meteo-pl"));
    QGuiApplication::setApplicationDisplayName(Meteo::i18n("Meteo · ICM meteograms"));
    QGuiApplication::setDesktopFileName(QStringLiteral("pl.meteo.desktop"));
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
#ifdef Q_OS_ANDROID
        QQuickStyle::setStyle(QStringLiteral("Material"));
#else
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
#endif
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(Meteo::i18n("meteo.pl meteograms for the KDE desktop"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), Meteo::i18n("Check QML startup without downloading data.")});
    parser.addOption({QStringLiteral("location"), Meteo::i18n("Location to display."), QStringLiteral("name")});
    parser.addOption({QStringLiteral("latitude"), Meteo::i18n("Latitude of the location."), QStringLiteral("degrees")});
    parser.addOption({QStringLiteral("longitude"), Meteo::i18n("Longitude of the location."), QStringLiteral("degrees")});
    parser.addOption({QStringLiteral("model"), Meteo::i18n("Forecast model to display."), QStringLiteral("model")});
    parser.process(app);
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    QVariantMap startupForecast;
    if (parser.isSet("location") || parser.isSet("latitude") || parser.isSet("longitude") || parser.isSet("model")) {
        bool latitudeOk = false, longitudeOk = false;
        const auto latitude = parser.value("latitude").toDouble(&latitudeOk);
        const auto longitude = parser.value("longitude").toDouble(&longitudeOk);
        const auto name = parser.value("location").trimmed();
        const auto model = parser.value("model");
        if (name.isEmpty() || !latitudeOk || !longitudeOk || !Meteo::validLocation(latitude, longitude, model)) {
            qCritical().noquote() << Meteo::i18n("Specify a location name, valid coordinates and a supported model together.");
            return 2;
        }
        startupForecast = {{"locationName", name}, {"latitude", latitude}, {"longitude", longitude}, {"model", model}};
    }
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("smokeTest"), smokeTest}, {QStringLiteral("startupForecast"), startupForecast}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("pl.meteo.app"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;
    if (smokeTest)
        QTimer::singleShot(1000, &app, &QCoreApplication::quit);
    return app.exec();
}
