#include "presentation/SavedPlaceController.hpp"
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <array>

class PlacesTest : public QObject
{
    Q_OBJECT
private slots:
    void widgetsFilterAndOrderTheSharedPlacesIndependently()
    {
        QTemporaryDir directory;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        UserPreferences preferences;
        preferences.savePlace("Kielce", 50.8661, 20.6286);
        preferences.savePlace("Gdańsk", 54.352, 18.6466);
        preferences.savePlace("Warszawa", 52.2297, 21.0122);
        const auto kielce = SavedPlaceController::placeKey(50.8661, 20.6286);
        const auto gdansk = SavedPlaceController::placeKey(54.352, 18.6466);
        SavedPlaceController first, second;
        first.setPreferences(&preferences); second.setPreferences(&preferences);
        first.setVisibleKeys({kielce, gdansk}); first.setOrderKeys({gdansk, kielce});
        second.setVisibleKeys({kielce});
        QCOMPARE(first.places().size(), 2);
        QCOMPARE(first.places().first().toMap()["name"].toString(), "Gdańsk");
        QCOMPARE(second.places().size(), 1);
        WeatherModel weather; weather.setAutoRefresh(false);
        first.setWeather(&weather);
        connect(&first, &SavedPlaceController::locationSelected, &weather, [&](const QString &name, double lat, double lon) {
            weather.setLocationName(name); weather.setLatitude(lat); weather.setLongitude(lon);
        });
        first.componentComplete();
        QCOMPARE(weather.locationName(), "Gdańsk");
        preferences.savePlace("Gdańsk centrum", 54.352, 18.6466);
        QTRY_COMPARE(weather.locationName(), "Gdańsk centrum");
        first.setVisibleKeys({kielce});
        QTRY_COMPARE(weather.locationName(), "Kielce");
        preferences.removePlace(50.8661, 20.6286);
        QTRY_VERIFY(weather.locationName().isEmpty());
        QVERIFY(second.places().isEmpty());
        QCOMPARE(preferences.favorites().size(), 2);
        first.setVisibleKeys({"*"});
        QTRY_COMPARE(weather.locationName(), "Gdańsk centrum");
    }
    void threeViewsKeepIndependentSelectionsAndFollowCollectionChanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        UserPreferences preferences;
        preferences.savePlace("Kielce", 50.8661, 20.6286);
        preferences.savePlace("Gdańsk", 54.352, 18.6466);
        std::array<WeatherModel, 3> weather;
        std::array<SavedPlaceController, 3> selection;
        for (size_t i = 0; i < weather.size(); ++i) {
            auto *model = &weather[i];
            model->setAutoRefresh(false);
            model->setLocationName(i == 1 ? "Gdańsk" : "Kielce");
            model->setLatitude(i == 1 ? 54.352 : 50.8661);
            model->setLongitude(i == 1 ? 18.6466 : 20.6286);
            auto &controller = selection[i];
            controller.setWeather(model);
            controller.setPreferences(&preferences);
            connect(&controller, &SavedPlaceController::locationSelected, model,
                    [model](const QString &name, double latitude, double longitude) {
                        model->setLocationName(name);
                        model->setLatitude(latitude);
                        model->setLongitude(longitude);
                    });
            controller.componentComplete();
        }
        preferences.savePlace("Kielce centrum", 50.8661, 20.6286);
        QTRY_COMPARE(weather[0].locationName(), "Kielce centrum");
        QTRY_COMPARE(weather[2].locationName(), "Kielce centrum");
        QCOMPARE(weather[1].locationName(), "Gdańsk");
        // A new selection in the application leaves both widget selections alone.
        weather[0].setLocationName("Gdańsk");
        weather[0].setLatitude(54.352);
        weather[0].setLongitude(18.6466);
        QTest::qWait(10);
        QCOMPARE(weather[2].locationName(), "Kielce centrum");
        QCOMPARE(preferences.favorites().size(), 2);
        preferences.removePlace(54.352, 18.6466);
        QTRY_COMPARE(weather[0].locationName(), "Kielce centrum");
        QTRY_COMPARE(weather[1].locationName(), "Kielce centrum");
        preferences.removePlace(50.8661, 20.6286);
        for (const auto &model : weather)
            QTRY_VERIFY(model.locationName().isEmpty());
        QTest::qWait(10);
        QVERIFY(preferences.favorites().isEmpty()); // Controllers cannot resurrect deleted places.
        preferences.savePlace("Precise point", 51.123456789, 19.987654321);
        for (const auto &model : weather) {
            QTRY_COMPARE(model.locationName(), "Precise point");
            QCOMPARE(model.latitude(), 51.123456789);
            QCOMPARE(model.longitude(), 19.987654321);
        }
        QCOMPARE(preferences.placeAt(51.1234568, 19.9876543)["name"].toString(), "Precise point");
        QVERIFY(preferences.placeAt(51.12346, 19.98765).isEmpty());
    }
};
QTEST_GUILESS_MAIN(PlacesTest)
#include "test_places.moc"
