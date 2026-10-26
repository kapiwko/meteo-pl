pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import pl.meteo.desktop

PlasmoidItem {
    id: root
    property string launchError: ""
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
        if (name.length) Plasmoid.configuration.model = LocationRules.modelFor(latitude, longitude, Plasmoid.configuration.model);
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
    fullRepresentation: PlasmaExtras.Representation {
        id: fullView
        readonly property bool charts: root.weatherConfiguration.popupView === "charts"
        Layout.minimumWidth: Kirigami.Units.gridUnit * 20
        Layout.minimumHeight: Kirigami.Units.gridUnit * 14
        Layout.maximumHeight: Screen.height * 0.85
        implicitWidth: Kirigami.Units.gridUnit * (charts ? 30 : 24)
        implicitHeight: Math.min(Screen.height * 0.85,
            Math.max(Kirigami.Units.gridUnit * 18, contentPane.implicitHeight
                + (header.visible ? header.implicitHeight : 0) + footer.implicitHeight))
        collapseMarginsHint: true

        header: PlasmaExtras.BasicPlasmoidHeading {
            leftPadding: Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.smallSpacing
            PlasmaComponents.ToolButton {
                id: refreshButton
                icon.name: "view-refresh"
                text: I18n.i18n("Refresh")
                display: AbstractButton.IconOnly
                enabled: !forecastModel.busy
                onClicked: forecastModel.refresh()
                PlasmaComponents.ToolTip { text: refreshButton.text }
            }
        }
        footer: PlasmaExtras.PlasmoidHeading {
            position: ToolBar.Footer
            leftPadding: Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.smallSpacing
            contentItem: RowLayout {
                Item { Layout.fillWidth: true }
                PlasmaComponents.Button {
                    objectName: "openMeteogramButton"
                    text: I18n.i18n("Show meteogram")
                    icon.name: "window-new"
                    onClicked: {
                        if (ApplicationLauncher.openForecast(forecastModel.locationName, forecastModel.latitude,
                            forecastModel.longitude, forecastModel.model)) {
                            root.launchError = "";
                            root.expanded = false;
                        } else {
                            root.launchError = I18n.i18n("Could not open Meteo. Install the desktop application and try again.");
                        }
                    }
                }
            }
        }
        contentItem: Pane {
            id: contentPane
            padding: Kirigami.Units.largeSpacing
            background: null
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.launchError
                    type: Kirigami.MessageType.Error
                    showCloseButton: true
                    onVisibleChanged: if (!visible) root.launchError = ""
                }
                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: fullView.charts ? widgetCharts : overview
                }
            }
        }
        Component {
            id: overview
            PlasmaComponents.ScrollView {
                id: overviewScroll
                implicitHeight: dailyForecast.implicitHeight
                contentWidth: availableWidth
                clip: true
                PlasmaComponents.ScrollBar.horizontal.policy: PlasmaComponents.ScrollBar.AlwaysOff
                CompactForecast {
                    id: dailyForecast
                    width: overviewScroll.availableWidth
                    weather: forecastModel
                    onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude)
                }
            }
        }
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
    }
}
