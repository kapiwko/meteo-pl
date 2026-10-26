pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Item {
    id: root
    required property WeatherModel weather
    readonly property bool loading: (!weather.hasData && weather.locationName.length > 0
        && (weather.busy || (weather.autoRefresh && !weather.error.length)))
        || (weather.hasData && (weatherIcon.status === Kirigami.Icon.Null || weatherIcon.status === Kirigami.Icon.Loading))
    implicitWidth: Kirigami.Units.iconSizes.large
    implicitHeight: implicitWidth
    Accessible.role: Accessible.StaticText
    Accessible.name: loading ? I18n.i18n("Downloading forecast") : weather.hasData ? weather.summaryTime : I18n.i18n("No forecast")
    Kirigami.Icon {
        id: weatherIcon
        objectName: "forecastWeatherIcon"
        anchors.fill: parent
        source: root.weather.hasData ? root.weather.weatherIcon : ""
        placeholder: ""
        fallback: "weather-none-available"
        animated: false
        visible: root.weather.hasData
        opacity: status === Kirigami.Icon.Ready ? 1 : 0
        Accessible.ignored: true
    }
    BusyIndicator {
        objectName: "forecastLoadingIndicator"
        anchors.fill: parent
        running: root.loading
        visible: running
        Accessible.ignored: true
    }
}
