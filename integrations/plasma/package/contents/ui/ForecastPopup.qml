// Plasma before 6.6 reads index from the delegate context; newer versions
// expose a native index property. Keep that context and bind the native slot
// when available, without redeclaring and shadowing it.
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
    property int legendOverride: -1
    readonly property bool legendVisible: legendOverride < 0 ? UserPreferences.showLegend : Boolean(legendOverride)
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
        objectName: "widgetChartSelection"
        weather: root.weather
        profile: root.profile
        hours: root.hours
        clockTime: root.clockTime
    }
    Timer { interval: 60000; running: true; repeat: true; onTriggered: root.clockTime = Date.now() / 1000 }
    contentItem: ListView {
        id: sections
        objectName: "widgetSections"
        model: root.weather.hasData && root.showCharts ? selection.charts : []
        spacing: Kirigami.Units.smallSpacing
        currentIndex: -1
        cacheBuffer: 1000
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        highlight: PlasmaExtras.Highlight {}
        highlightMoveDuration: Kirigami.Units.shortDuration
        highlightResizeDuration: Kirigami.Units.shortDuration
        header: ColumnLayout {
            width: sections.width
            spacing: Kirigami.Units.largeSpacing
            function chooseLocation() { summary.chooseLocation(); }
            ProfileUpdateNotice { Layout.fillWidth: true; profileId: root.profile }
            CompactForecast {
                id: summary
                Layout.fillWidth: true
                weather: root.weather
                places: root.places
                showDays: !root.showCharts
                onAddLocationRequested: root.configureRequested()
                onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude)
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.weather.hasData && root.showCharts
                PlasmaComponents.Label {
                    objectName: "widgetForecastPeriod"
                    Layout.fillWidth: true
                    text: root.weather.formatTime(selection.startTime, "ddd d MMM HH:mm") + " – "
                        + root.weather.formatTime(selection.endTime, "ddd d MMM HH:mm") + (root.weather.utc ? " UTC" : "")
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                }
                PlasmaComponents.ToolButton {
                    id: legendButton
                    objectName: "widgetLegendToggle"
                    icon.name: "view-list-details"
                    text: root.legendVisible ? I18n.i18n("Hide legend") : I18n.i18n("Show legend")
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: root.legendVisible
                    onClicked: root.legendOverride = checked ? 1 : 0
                    PlasmaComponents.ToolTip { text: legendButton.text }
                }
            }
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
            readonly property var modelData: model.modelData
            objectName: "widgetSection-" + modelData.key
            title: modelData.title
            icon: "view-statistics"
            onItemExpanded: root.sectionToggled(modelData.key, true)
            onItemCollapsed: root.sectionToggled(modelData.key, false)
            Component.onCompleted: {
                if ("index" in section) section.index = Qt.binding(() => model.index);
                if (root.collapsedCharts.indexOf(modelData.key) < 0) expand();
            }
            customExpandedViewContent: chartContent
            Component {
                id: chartContent
                Meteogram {
                    weather: root.weather
                    title: section.modelData.title
                    groupKey: section.modelData.key
                    fields: section.modelData.fields
                    chartConfiguration: section.modelData
                    showHeading: false
                    legendOverride: root.legendOverride
                    chartHeight: section.modelData.height || UserPreferences.chartHeightForProfile(root.profile) || UserPreferences.chartHeight
                    startTime: selection.startTime
                    endTime: selection.endTime
                    nights: UserPreferences.showNight ? root.weather.nightIntervals : []
                }
            }
        }
    }
}
