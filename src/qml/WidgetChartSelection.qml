pragma ComponentBehavior: Bound
import QtQml

QtObject {
    id: root
    required property WeatherModel weather
    property string profile: "classic"
    property int hours: 24
    property real clockTime: Date.now() / 1000
    readonly property real startTime: hours === 0 ? weather.firstTimestamp : Math.max(weather.firstTimestamp,
        Math.min(Math.floor(clockTime / 3600) * 3600, weather.lastTimestamp - Math.min(hours * 3600, weather.lastTimestamp - weather.firstTimestamp)))
    readonly property real endTime: hours === 0 ? weather.lastTimestamp : Math.min(weather.lastTimestamp, startTime + hours * 3600)
    readonly property var charts: {
        if (!weather.hasData || !Object.keys(weather.series).length) return [];
        const revision = UserPreferences.chartLayout;
        return weather.configuredCharts(UserPreferences.chartLayoutForProfile(profile));
    }
}
