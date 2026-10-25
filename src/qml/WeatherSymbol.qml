pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Item {
    id: root
    required property WeatherModel weather
    readonly property bool loading: !weather.hasData && weather.locationName.length > 0
        && (weather.busy || (weather.autoRefresh && !weather.error.length))
    implicitWidth: Kirigami.Units.iconSizes.large
    implicitHeight: implicitWidth
    Accessible.role: Accessible.StaticText
    Accessible.name: loading ? I18n.i18n("Downloading forecast") : weather.hasData ? weather.summaryTime : I18n.i18n("No forecast")
    Kirigami.Icon {
        objectName: "forecastWeatherIcon"
        anchors.fill: parent
        source: root.weather.hasData ? root.weather.weatherIcon : ""
        visible: root.weather.hasData
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
