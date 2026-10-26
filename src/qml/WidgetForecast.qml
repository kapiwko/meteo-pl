pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property string profile: "classic"
    property var chartKeys: ["temperature", "precipitation"]
    property int hours: 24
    property var places: UserPreferences.places
    function chooseLocation() { summary.chooseLocation(); }
    property real clockTime: Date.now() / 1000
    signal locationSelected(string name, real latitude, real longitude)
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
    spacing: Kirigami.Units.largeSpacing
    Timer { interval: 60000; running: true; repeat: true; onTriggered: root.clockTime = Date.now() / 1000 }
    CompactForecast {
        id: summary
        Layout.fillWidth: true
        Layout.fillHeight: false
        weather: root.weather
        places: root.places
        showDays: false
        onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude)
    }
    Label {
        Layout.fillWidth: true
        visible: root.weather.hasData
        text: root.weather.formatTime(root.startTime, "ddd HH:mm") + " – " + root.weather.formatTime(root.endTime, "ddd HH:mm")
        color: Kirigami.Theme.disabledTextColor
        font: Kirigami.Theme.smallFont
    }
    ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitHeight: chartColumn.implicitHeight
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            id: chartColumn
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing
            Repeater {
                objectName: "widgetChartRepeater"
                model: root.weather.hasData ? root.charts : []
                delegate: Meteogram {
                    required property var modelData
                    Layout.fillWidth: true
                    weather: root.weather
                    title: modelData.title
                    groupKey: modelData.key
                    fields: modelData.fields
                    chartConfiguration: modelData
                    chartHeight: modelData.height || UserPreferences.chartHeightForProfile(root.profile) || UserPreferences.chartHeight
                    startTime: root.startTime
                    endTime: root.endTime
                    nights: UserPreferences.showNight ? root.weather.nightIntervals : []
                }
            }
        }
    }
}
