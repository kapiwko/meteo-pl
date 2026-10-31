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
        property bool followCurrentLocation: false
    }
    property WeatherModel forecast: WeatherModel {
        id: forecastModel
        objectName: "appForecastModel"
        locationName: session.temporaryLocation ? session.startupForecast.locationName : settings.locationName
        latitude: session.temporaryLocation ? session.startupForecast.latitude : settings.latitude
        longitude: session.temporaryLocation ? session.startupForecast.longitude : settings.longitude
        model: session.temporaryLocation ? session.startupForecast.model : settings.model
        autoRefresh: session.startupReady && !session.smokeTest && locationName.length > 0
            && (!currentLocation.enabled || currentLocation.hasPosition)
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
    function selectLocation(name, latitude, longitude) {
        selectForecast(name, latitude, longitude, forecastModel.model);
    }
    Component.onCompleted: {
        forecastModel.currentLocation.enabled = !smokeTest && !temporaryLocation && settings.followCurrentLocation;
        startupReady = true;
    }
    property Connections tracking: Connections {
        target: forecastModel.currentLocation
        function onStateChanged() {
            if (forecastModel.currentLocation.enabled) session.temporaryLocation = false;
            if (!session.smokeTest) settings.followCurrentLocation = forecastModel.currentLocation.enabled;
        }
    }
    property SavedPlaceSelection selection: SavedPlaceSelection { weather: session.startupReady && !session.temporaryLocation && !forecastModel.currentLocation.enabled ? forecastModel : null; onLocationSelected: (name, latitude, longitude) => session.selectLocation(name, latitude, longitude) }
    function selectModel(model) {
        settings.model = LocationRules.modelFor(forecastModel.latitude, forecastModel.longitude, model);
        forecastModel.model = Qt.binding(() => settings.model);
    }
    function selectForecast(name, latitude, longitude, model) {
        forecastModel.currentLocation.enabled = false;
        settings.model = name.length ? LocationRules.modelFor(latitude, longitude, model) : model;
        settings.locationName = name; settings.latitude = latitude; settings.longitude = longitude;
        temporaryLocation = false;
        // Device fixes update the model from C++. Choosing the previously
        // selected saved place may leave every Settings value unchanged, so
        // explicitly restore its bindings rather than relying on change signals.
        forecastModel.model = Qt.binding(() => settings.model);
        forecastModel.locationName = Qt.binding(() => settings.locationName);
        forecastModel.latitude = Qt.binding(() => settings.latitude);
        forecastModel.longitude = Qt.binding(() => settings.longitude);
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
}
