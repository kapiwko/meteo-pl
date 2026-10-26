pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import pl.meteo.desktop

PlasmaComponents.ScrollView {
    id: root
    required property WeatherModel weather
    property var places: UserPreferences.places
    property bool showCharts: true
    property string profile: "classic"
    property int hours: 24
    property bool detailsExpanded: false
    property var collapsedCharts: []
    property real clockTime: Date.now() / 1000
    signal locationSelected(string name, real latitude, real longitude)
    signal configureRequested()
    signal sectionToggled(string key, bool expanded)
    function chooseLocation() { if (sections.headerItem) sections.headerItem.chooseLocation(); }
    contentWidth: availableWidth
    clip: true
    PlasmaComponents.ScrollBar.horizontal.policy: PlasmaComponents.ScrollBar.AlwaysOff
    PlasmaComponents.ScrollBar.vertical.objectName: "widgetVerticalScrollBar"
    WidgetChartSelection {
        id: selection
        weather: root.weather
        profile: root.profile
        hours: root.hours
        clockTime: root.clockTime
    }
    Timer { interval: 60000; running: true; repeat: true; onTriggered: root.clockTime = Date.now() / 1000 }
    contentItem: ListView {
        id: sections
        objectName: "widgetSections"
        model: root.weather.hasData ? [{key: "details", title: I18n.i18n("Weather details")}]
            .concat(root.showCharts ? selection.charts : []) : []
        spacing: Kirigami.Units.smallSpacing
        currentIndex: -1
        cacheBuffer: 1000
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        highlight: PlasmaExtras.Highlight {}
        highlightMoveDuration: Kirigami.Units.shortDuration
        highlightResizeDuration: Kirigami.Units.shortDuration
        header: CompactForecast {
            width: sections.width
            weather: root.weather
            places: root.places
            showDays: !root.showCharts
            onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude)
        }
        footer: PlasmaExtras.PlaceholderMessage {
            width: sections.width
            visible: !root.weather.hasData && !root.weather.busy
            height: visible ? implicitHeight : 0
            text: root.weather.locationName.length ? I18n.i18n("No forecast available") : I18n.i18n("Choose location")
            iconName: "weather-none-available"
            helpfulAction: QQC2.Action {
                text: root.weather.locationName.length ? I18n.i18n("Refresh") : I18n.i18n("Location")
                icon.name: root.weather.locationName.length ? "view-refresh" : "mark-location"
                onTriggered: root.weather.locationName.length ? root.weather.refresh() : root.configureRequested()
            }
        }
        delegate: PlasmaExtras.ExpandableListItem {
            id: section
            required property var modelData
            required property int index
            objectName: "widgetSection-" + modelData.key
            property int legendOverride: -1
            title: modelData.title
            subtitle: modelData.key === "details" ? root.weather.modelName
                : root.weather.formatTime(selection.startTime, "ddd HH:mm") + " – " + root.weather.formatTime(selection.endTime, "ddd HH:mm")
            icon: modelData.key === "details" ? "weather-clear" : "view-statistics"
            onItemExpanded: root.sectionToggled(modelData.key, true)
            onItemCollapsed: root.sectionToggled(modelData.key, false)
            Component.onCompleted: {
                if (modelData.key === "details" ? root.detailsExpanded : root.collapsedCharts.indexOf(modelData.key) < 0) expand();
            }
            defaultActionButtonVisible: modelData.key !== "details"
            defaultActionButtonAction: modelData.key === "details" ? null : legendAction
            QQC2.Action {
                id: legendAction
                icon.name: "dialog-information-symbolic"
                text: I18n.i18n("Show legend")
                checkable: true
                checked: section.legendOverride < 0 ? UserPreferences.showLegend : Boolean(section.legendOverride)
                onTriggered: section.legendOverride = checked ? 1 : 0
            }
            customExpandedViewContent: modelData.key === "details" ? detailsContent : chartContent
            Component {
                id: chartContent
                Meteogram {
                    weather: root.weather
                    title: section.modelData.title
                    groupKey: section.modelData.key
                    fields: section.modelData.fields
                    chartConfiguration: section.modelData
                    showHeading: false
                    legendOverride: section.legendOverride
                    chartHeight: section.modelData.height || UserPreferences.chartHeightForProfile(root.profile) || UserPreferences.chartHeight
                    startTime: selection.startTime
                    endTime: selection.endTime
                    nights: UserPreferences.showNight ? root.weather.nightIntervals : []
                }
            }
            Component {
                id: detailsContent
                Item {
                    implicitHeight: details.implicitHeight + Kirigami.Units.largeSpacing
                    Kirigami.FormLayout {
                        id: details
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        PlasmaComponents.Label { Kirigami.FormData.label: I18n.i18n("Pressure"); text: root.weather.pressureText }
                        PlasmaComponents.Label { Kirigami.FormData.label: I18n.i18n("Wind"); text: root.weather.windText }
                        PlasmaComponents.Label { Kirigami.FormData.label: I18n.i18n("Sunrise"); text: root.weather.solarTimes.sunriseText || "—" }
                        PlasmaComponents.Label { Kirigami.FormData.label: I18n.i18n("Sunset"); text: root.weather.solarTimes.sunsetText || "—" }
                        PlasmaComponents.Label { Kirigami.FormData.label: I18n.i18n("Model run"); text: root.weather.runText; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
