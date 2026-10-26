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
    property bool temporaryLocation: startupForecast.locationName !== undefined
    property bool startupReady: false
    width: 860
    height: 900
    minimumWidth: Kirigami.Settings.isMobile ? 320 : 400
    minimumHeight: 440
    visible: true
    title: forecastModel.locationName + " · Meteo"

    Settings {
        id: settings
        objectName: "locationSettings"
        category: "Location"
        property string locationName: "Warszawa"
        property real latitude: 52.2297
        property real longitude: 21.0122
        property string model: "um4_60"
    }
    WeatherModel {
        id: forecastModel
        objectName: "appForecastModel"
        locationName: window.temporaryLocation ? window.startupForecast.locationName : settings.locationName
        latitude: window.temporaryLocation ? window.startupForecast.latitude : settings.latitude
        longitude: window.temporaryLocation ? window.startupForecast.longitude : settings.longitude
        model: window.temporaryLocation ? window.startupForecast.model : settings.model
        autoRefresh: window.startupReady && !window.smokeTest && locationName.length > 0
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
    function selectLocation(name, latitude, longitude) {
        selectForecast(name, latitude, longitude, forecastModel.model);
    }
    Component.onCompleted: startupReady = true
    SavedPlaceSelection { weather: window.startupReady && !window.temporaryLocation ? forecastModel : null; onLocationSelected: (name, latitude, longitude) => window.selectLocation(name, latitude, longitude) }
    function selectForecast(name, latitude, longitude, model) {
        settings.model = name.length ? LocationRules.modelFor(latitude, longitude, model) : model;
        settings.locationName = name; settings.latitude = latitude; settings.longitude = longitude;
        temporaryLocation = false;
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
}
