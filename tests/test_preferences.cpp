#include "UserPreferences.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class PreferencesTest : public QObject {
    Q_OBJECT
private slots:
    void anotherProcessUpdatesExistingQmlBindings()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml
            import pl.meteo.desktop
            QtObject {
                property string renderer: UserPreferences.renderer
                property string unit: UserPreferences.windUnit
                property bool utc: UserPreferences.utc
                property bool legend: UserPreferences.showLegend
                property int chartHeight: UserPreferences.chartHeight
                property string profile: UserPreferences.profile
                property int places: UserPreferences.places.length
            }
        )", {});
        QScopedPointer<QObject> bindings(component.create());
        QVERIFY2(bindings, qPrintable(component.errorString()));
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(preferences);
        QCOMPARE(bindings->property("renderer").toString(), "gpu");
        QSignalSpy display(preferences, &UserPreferences::displayChanged);
        QSignalSpy places(preferences, &UserPreferences::placesChanged);
        QProcess writer;
        writer.start(QCoreApplication::applicationFilePath(), {"--write-settings", preferences->settingsFile()});
        QVERIFY(writer.waitForFinished());
        QCOMPARE(writer.exitCode(), 0);
        QTRY_COMPARE(bindings->property("renderer").toString(), "cpu");
        QCOMPARE(bindings->property("unit").toString(), "km/h");
        QCOMPARE(bindings->property("utc").toBool(), true);
        QCOMPARE(bindings->property("legend").toBool(), false);
        QCOMPARE(bindings->property("chartHeight").toInt(), 262);
        QCOMPARE(bindings->property("profile").toString(), "full");
        QCOMPARE(bindings->property("places").toInt(), 1);
        QCOMPARE(display.count(), 1);
        QCOMPARE(places.count(), 1);
        preferences->setRenderer("gpu");
        QTest::qWait(150);
        QCOMPARE(display.count(), 2); // No duplicate from watching our own atomic write.
        QCOMPARE(places.count(), 1);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().value(1) == "--write-settings") {
        QSettings settings(application.arguments().value(2), QSettings::IniFormat);
        settings.setValue("display/renderer", "cpu");
        settings.setValue("display/windUnit", "km/h");
        settings.setValue("display/utc", true);
        settings.setValue("display/showLegend", false);
        settings.setValue("display/chartHeight", 262);
        settings.setValue("display/profile", "full");
        settings.setValue("places/favorites", QByteArray(R"([{"name":"Kielce","latitude":50.8661,"longitude":20.6286}])"));
        settings.sync();
        return settings.status() == QSettings::NoError ? 0 : 1;
    }
    PreferencesTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_preferences.moc"
