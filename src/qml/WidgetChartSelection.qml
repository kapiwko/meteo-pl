pragma ComponentBehavior: Bound
import QtQml

QtObject {
    id: root
    required property WeatherModel weather
    property string profile: "classic"
    property var chartKeys: ["temperature", "precipitation"]
    property int hours: 24
    property real clockTime: Date.now() / 1000
    readonly property real startTime: hours === 0 ? weather.firstTimestamp : Math.max(weather.firstTimestamp,
        Math.min(Math.floor(clockTime / 3600) * 3600, weather.lastTimestamp - Math.min(hours * 3600, weather.lastTimestamp - weather.firstTimestamp)))
    readonly property real endTime: hours === 0 ? weather.lastTimestamp : Math.min(weather.lastTimestamp, startTime + hours * 3600)
    readonly property var charts: {
        if (!weather.hasData || !Object.keys(weather.series).length) return [];
        const revision = UserPreferences.chartLayout;
        const available = weather.configuredCharts(UserPreferences.chartLayoutForProfile(profile));
        const selected = chartKeys.map(key => available.find(chart => chart.key === key)).filter(chart => chart !== undefined).slice(0, 2);
        return selected.length ? selected : available.slice(0, 2);
    }
}
