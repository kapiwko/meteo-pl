pragma ComponentBehavior: Bound
import QtQuick
import QtCore
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

Kirigami.ApplicationWindow {
    id: window
    readonly property alias weather: forecastModel
    property bool smokeTest: false
    property var startupForecast: ({})
    property bool startupReady: false
    width: 860
    height: 900
    minimumWidth: Kirigami.Settings.isMobile ? 320 : 400
    minimumHeight: 440
    visible: true
    title: forecastModel.locationName + " · Meteo"

    Settings {
        id: settings
        category: "Location"
        property string locationName: "Warszawa"
        property real latitude: 52.2297
        property real longitude: 21.0122
        property string model: "um4_60"
    }
    WeatherModel {
        id: forecastModel
        objectName: "appForecastModel"
        locationName: settings.locationName
        latitude: settings.latitude
        longitude: settings.longitude
        model: settings.model
        autoRefresh: window.startupReady && !window.smokeTest && settings.locationName.length > 0
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
    function selectLocation(name, latitude, longitude) {
        settings.locationName = name; settings.latitude = latitude; settings.longitude = longitude;
        if (name.length) settings.model = LocationRules.modelFor(latitude, longitude, settings.model);
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
    Component.onCompleted: {
        if (startupForecast.locationName !== undefined) {
            settings.model = startupForecast.model;
            selectLocation(startupForecast.locationName, startupForecast.latitude, startupForecast.longitude);
        }
        startupReady = true;
    }
    SavedPlaceSelection { weather: window.startupReady ? forecastModel : null; onLocationSelected: (name, latitude, longitude) => window.selectLocation(name, latitude, longitude) }
    function selectForecast(name, latitude, longitude, model) {
        settings.model = model;
        selectLocation(name, latitude, longitude);
    }
}
