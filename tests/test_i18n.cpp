#include "core/Localization.hpp"
#include "core/Forecast.hpp"
#include "ForecastFields.hpp"
#include "UserPreferences.hpp"
#include "ChartConfiguration.hpp"
#include <KLocalizedString>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QLocale>
#include <QTest>

class TranslationTest : public QObject {
    Q_OBJECT
    bool polish = false;
private slots:
    void initTestCase()
    {
        const auto language = qEnvironmentVariable("METEO_TEST_LANGUAGE");
        polish = language.startsWith("pl");
        QLocale::setDefault(QLocale(language));
        KLocalizedString::setLanguages({language, language.section('_', 0, 0)});
        // The shared module must not replace the host's translation domain.
        KLocalizedString::setApplicationDomain("");
    }
    void translatesNativeErrorsAndCatalogs()
    {
        QCOMPARE(Meteo::i18n("Settings"), polish ? QStringLiteral("Ustawienia") : QStringLiteral("Settings"));
        QCOMPARE(forecastFields().first().label, polish ? QStringLiteral("Temperatura powietrza") : QStringLiteral("Air temperature"));
        QString error;
        QVERIFY(!Meteo::parseForecast("{}", &error));
        QCOMPARE(error, polish ? QStringLiteral("Brak temperatury lub daty uruchomienia modelu.") : QStringLiteral("Missing temperature or model run date."));
        QVERIFY(KLocalizedString::applicationDomain().isEmpty());
        // QString::arg callers receive complete placeholders, without KI18n diagnostics.
        QCOMPARE(Meteo::i18n("Code %1").arg(17), polish ? QStringLiteral("Kod 17") : QStringLiteral("Code 17"));
    }
    void supportsPolishPluralForms()
    {
        QCOMPARE(Meteo::i18np("%1 chart selected", "%1 charts selected", 1), polish ? QStringLiteral("Wybrano 1 wykres") : QStringLiteral("1 chart selected"));
        QCOMPARE(Meteo::i18np("%1 chart selected", "%1 charts selected", 2), polish ? QStringLiteral("Wybrano 2 wykresy") : QStringLiteral("2 charts selected"));
        QCOMPARE(Meteo::i18np("%1 chart selected", "%1 charts selected", 5), polish ? QStringLiteral("Wybrano 5 wykresów") : QStringLiteral("5 charts selected"));
        QCOMPARE(Meteo::i18np("%1 chart selected", "%1 charts selected", 12), polish ? QStringLiteral("Wybrano 12 wykresów") : QStringLiteral("12 charts selected"));
    }
    void translatesBundledJsonProfileNames()
    {
        const auto profiles = builtinChartProfiles();
        QCOMPARE(profiles.first().toMap()["name"].toString(), polish ? QStringLiteral("Klasyczny ICM") : QStringLiteral("Classic ICM"));
        QCOMPARE(profiles[1].toMap()["name"].toString(), polish ? QStringLiteral("Skrót prognozy") : QStringLiteral("Quick forecast"));
    }
    void translatesEachQmlEngine()
    {
        for (int i = 0; i < 2; ++i) {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(R"(
                import QtQuick
                import pl.meteo.desktop
                Item {
                    property string caption: I18n.i18n("Settings")
                    property string place: I18n.i18n("Show forecast")
                    property string count: I18n.i18np("%1 chart selected", "%1 charts selected", 22)
                }
            )", QUrl());
            QScopedPointer<QObject> object(component.create());
            QVERIFY2(object, qPrintable(component.errorString()));
            QCOMPARE(object->property("caption").toString(), polish ? QStringLiteral("Ustawienia") : QStringLiteral("Settings"));
            QCOMPARE(object->property("place").toString(), polish ? QStringLiteral("Pokaż prognozę") : QStringLiteral("Show forecast"));
            QCOMPARE(object->property("count").toString(), polish ? QStringLiteral("Wybrano 22 wykresy") : QStringLiteral("22 charts selected"));
        }
        QVERIFY(KLocalizedString::applicationDomain().isEmpty());
    }
};
QTEST_MAIN(TranslationTest)
#include "test_i18n.moc"
