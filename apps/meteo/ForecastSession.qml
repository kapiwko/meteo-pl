pragma ComponentBehavior: Bound
import QtQuick
import QtCore
import pl.meteo.desktop

QtObject {
    id: session
    readonly property alias weather: forecastModel
    property bool smokeTest: false
    property var startupForecast: ({})
    property bool temporaryLocation: startupForecast.locationName !== undefined
    property bool startupReady: false
    property Settings settingsStore: Settings {
        id: settings
        objectName: "locationSettings"
        category: "Location"
        property string locationName: ""
        property real latitude: 0
        property real longitude: 0
        property string model: "um4_60"
    }
    property WeatherModel forecast: WeatherModel {
        id: forecastModel
        objectName: "appForecastModel"
        locationName: session.temporaryLocation ? session.startupForecast.locationName : settings.locationName
        latitude: session.temporaryLocation ? session.startupForecast.latitude : settings.latitude
        longitude: session.temporaryLocation ? session.startupForecast.longitude : settings.longitude
        model: session.temporaryLocation ? session.startupForecast.model : settings.model
        autoRefresh: session.startupReady && !session.smokeTest && locationName.length > 0
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
    function selectLocation(name, latitude, longitude) {
        selectForecast(name, latitude, longitude, forecastModel.model);
    }
    Component.onCompleted: startupReady = true
    property SavedPlaceSelection selection: SavedPlaceSelection { weather: session.startupReady && !session.temporaryLocation ? forecastModel : null; onLocationSelected: (name, latitude, longitude) => session.selectLocation(name, latitude, longitude) }
    function selectForecast(name, latitude, longitude, model) {
        settings.model = name.length ? LocationRules.modelFor(latitude, longitude, model) : model;
        settings.locationName = name; settings.latitude = latitude; settings.longitude = longitude;
        temporaryLocation = false;
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
}
