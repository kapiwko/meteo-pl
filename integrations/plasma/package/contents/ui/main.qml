pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import pl.meteo.desktop

PlasmoidItem {
    id: root
    property bool showMeteogram: false
    readonly property var weatherConfiguration: Plasmoid.configuration
    switchWidth: Kirigami.Units.gridUnit * 24
    switchHeight: Kirigami.Units.gridUnit * 24
    toolTipMainText: forecastModel.locationName + " · " + forecastModel.temperatureText
    toolTipSubText: forecastModel.summaryTime + "\n" + forecastModel.modelName
    Plasmoid.icon: forecastModel.hasData ? forecastModel.weatherIcon : Qt.resolvedUrl("../images/meteo.svg")
    Plasmoid.status: PlasmaCore.Types.ActiveStatus

    WeatherModel {
        id: forecastModel
        autoRefresh: locationName.length > 0
        latitude: root.weatherConfiguration.latitude
        longitude: root.weatherConfiguration.longitude
        locationName: root.weatherConfiguration.locationName
        model: root.weatherConfiguration.model
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
    function selectLocation(name, latitude, longitude) {
        Plasmoid.configuration.locationName = name;
        Plasmoid.configuration.latitude = latitude;
        Plasmoid.configuration.longitude = longitude;
        if (name.length && !(latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176)) Plasmoid.configuration.model = "gfs";
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
    SavedPlaceSelection { weather: forecastModel; onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude) }
    compactRepresentation: MouseArea {
        implicitWidth: compactRow.implicitWidth
        implicitHeight: Kirigami.Units.iconSizes.smallMedium
        Layout.minimumWidth: compactRow.implicitWidth
        Layout.minimumHeight: Kirigami.Units.iconSizes.smallMedium
        Accessible.name: root.toolTipMainText
        Accessible.role: Accessible.Button
        onClicked: root.expanded = !root.expanded
        RowLayout {
            id: compactRow
            anchors.centerIn: parent
            spacing: 3
            Item {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                WeatherSymbol { id: panelSymbol; anchors.fill: parent; weather: forecastModel }
                Kirigami.Icon { anchors.fill: parent; visible: !forecastModel.hasData && !panelSymbol.loading; source: Qt.resolvedUrl("../images/meteo.svg") }
            }
            Label { text: forecastModel.temperatureText }
        }
    }
    fullRepresentation: Item {
        id: fullView
        readonly property ForecastPanel chartView: forecastView.item as ForecastPanel
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20
        Layout.minimumHeight: Kirigami.Units.gridUnit * (root.showMeteogram ? 18 : 12)
        Layout.preferredWidth: Kirigami.Units.gridUnit * (root.showMeteogram ? 34 : root.weatherConfiguration.popupView === "charts" ? 30 : 24)
        Layout.preferredHeight: root.showMeteogram ? Kirigami.Units.gridUnit * 38 : Math.min(Screen.height * 0.85, overviewContent.implicitHeight + Kirigami.Units.largeSpacing * 2)
        implicitHeight: Layout.preferredHeight
        ColumnLayout {
            id: overviewContent
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing
            RowLayout {
                Layout.fillWidth: true
                ToolButton { icon.name: "go-previous"; text: I18n.i18n("Weather"); visible: root.showMeteogram; onClicked: root.showMeteogram = false }
                Kirigami.Heading { text: "Meteo"; level: 3; Layout.fillWidth: true }
                ToolButton { icon.name: "view-refresh"; text: I18n.i18n("Refresh"); display: AbstractButton.IconOnly; enabled: !forecastModel.busy; onClicked: forecastModel.refresh(); ToolTip.text: text; ToolTip.visible: hovered }
                ToolButton { icon.name: "configure"; text: root.showMeteogram ? I18n.i18n("Settings") : I18n.i18n("Widget settings"); display: AbstractButton.IconOnly; onClicked: root.showMeteogram ? displayDialog.open() : Plasmoid.internalAction("configure").trigger(); ToolTip.text: text; ToolTip.visible: hovered }
            }
            Loader {
                id: forecastView
                Layout.fillWidth: true
                Layout.fillHeight: root.showMeteogram || root.weatherConfiguration.popupView === "charts"
                Layout.preferredHeight: implicitHeight
                sourceComponent: root.showMeteogram ? details : root.weatherConfiguration.popupView === "charts" ? widgetCharts : overview
            }
            Component { id: overview; CompactForecast { weather: forecastModel; onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude) } }
            Component {
                id: widgetCharts
                WidgetForecast {
                    weather: forecastModel
                    profile: root.weatherConfiguration.chartProfile
                    chartKeys: Array.from(root.weatherConfiguration.chartKeys)
                    hours: root.weatherConfiguration.chartHours
                    onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude)
                }
            }
            Component {
                id: details
                ForecastPanel {
                    weather: forecastModel
                    onConfigureRequested: Plasmoid.internalAction("configure").trigger()
                    onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude)
                }
            }
            Button { Layout.alignment: Qt.AlignHCenter; visible: !root.showMeteogram; text: I18n.i18n("Show meteogram"); icon.name: "view-statistics"; onClicked: root.showMeteogram = true }
        }
        MeteoSettings {
            id: displayDialog
            parent: fullView
            width: Math.min(parent.width - 24, 650)
            height: Math.min(parent.height - 24, 680)
            weather: forecastModel
            showTimeRange: fullView.chartView !== null && fullView.chartView.viewMode === 0
            startTime: fullView.chartView ? fullView.chartView.viewportStart : forecastModel.firstTimestamp
            endTime: fullView.chartView ? fullView.chartView.viewportEnd : forecastModel.lastTimestamp
            onHoursSelected: hours => { if (fullView.chartView) fullView.chartView.setHours(hours); }
        }
    }
}
