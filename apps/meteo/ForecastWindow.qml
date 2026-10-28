pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: window
    readonly property alias weather: session.weather
    property alias smokeTest: session.smokeTest
    property alias startupForecast: session.startupForecast
    property alias temporaryLocation: session.temporaryLocation
    property alias startupReady: session.startupReady
    width: 860
    height: 900
    minimumWidth: Kirigami.Settings.isMobile ? 320 : 400
    minimumHeight: 440
    visible: true
    title: weather.locationName.length ? weather.locationName + " · Meteo" : "Meteo"
    ForecastSession { id: session }
    function selectLocation(name, latitude, longitude) { session.selectLocation(name, latitude, longitude); }
    function selectForecast(name, latitude, longitude, model) { session.selectForecast(name, latitude, longitude, model); }
}
