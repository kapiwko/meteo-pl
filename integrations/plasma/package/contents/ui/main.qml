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
        latitude: root.weatherConfiguration.latitude
        longitude: root.weatherConfiguration.longitude
        locationName: root.weatherConfiguration.locationName
        model: root.weatherConfiguration.model
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
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
            Kirigami.Icon { source: forecastModel.hasData ? forecastModel.weatherIcon : Qt.resolvedUrl("../images/meteo.svg"); Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
            Label { text: forecastModel.temperatureText }
        }
    }
    fullRepresentation: ColumnLayout {
        id: fullView
        readonly property ForecastPanel chartView: forecastView.item as ForecastPanel
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20
        Layout.minimumHeight: Kirigami.Units.gridUnit * 18
        Layout.preferredWidth: Kirigami.Units.gridUnit * 34
        Layout.preferredHeight: Kirigami.Units.gridUnit * (root.showMeteogram ? 38 : 23)
        RowLayout {
            Layout.fillWidth: true
            ToolButton { icon.name: "go-previous"; text: "Pogoda"; visible: root.showMeteogram; onClicked: root.showMeteogram = false }
            Label { text: "Meteo · ICM"; font.bold: true; Layout.fillWidth: true }
            ToolButton { icon.name: "view-refresh"; text: "Odśwież"; display: AbstractButton.IconOnly; enabled: !forecastModel.busy; onClicked: forecastModel.refresh(); ToolTip.text: text; ToolTip.visible: hovered }
            ToolButton { icon.name: "configure"; text: "Ustawienia"; display: AbstractButton.IconOnly; onClicked: displayDialog.open(); ToolTip.text: text; ToolTip.visible: hovered }
            ToolButton { icon.name: "mark-location"; text: "Lokalizacja"; display: AbstractButton.IconOnly; onClicked: Plasmoid.internalAction("configure").trigger(); ToolTip.text: text; ToolTip.visible: hovered }
            ToolButton { icon.name: "internet-web-browser"; text: "Otwórz meteo.pl"; display: AbstractButton.IconOnly; onClicked: Qt.openUrlExternally("https://app.meteo.pl/meteogram"); ToolTip.text: text; ToolTip.visible: hovered }
        }
        Loader {
            id: forecastView
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: root.showMeteogram ? details : overview
        }
        Component { id: overview; CompactForecast { weather: forecastModel } }
        Component {
            id: details
            ForecastPanel {
                weather: forecastModel
                onConfigureRequested: Plasmoid.internalAction("configure").trigger()
                onLocationSelected: (name, latitude, longitude) => {
                    Plasmoid.configuration.locationName = name;
                    Plasmoid.configuration.latitude = latitude;
                    Plasmoid.configuration.longitude = longitude;
                    if (!(latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176)) Plasmoid.configuration.model = "gfs";
                    UserPreferences.rememberPlace(name, latitude, longitude);
                }
            }
        }
        Button { Layout.alignment: Qt.AlignHCenter; visible: !root.showMeteogram; text: "Pokaż meteogram"; icon.name: "view-statistics"; onClicked: root.showMeteogram = true }
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
