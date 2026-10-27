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
    hideOnWindowDeactivate: !weatherConfiguration.pinned
    switchWidth: Kirigami.Units.gridUnit * 24
    switchHeight: Kirigami.Units.gridUnit * 24
    toolTipMainText: forecastModel.locationName + " · " + forecastModel.temperatureText
    toolTipSubText: forecastModel.summaryTime + "\n" + forecastModel.modelName
    Plasmoid.icon: forecastModel.hasData ? forecastModel.weatherIcon : "pl.meteo.desktop"
    Plasmoid.status: PlasmaCore.Types.ActiveStatus
    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            id: refreshAction
            objectName: "refreshForecastAction"
            text: I18n.i18n("Refresh")
            icon.name: "view-refresh"
            enabled: !forecastModel.busy && forecastModel.locationName.length > 0
            onTriggered: forecastModel.refresh()
        },
        PlasmaCore.Action {
            id: pinAction
            objectName: "pinPopupAction"
            text: I18n.i18n("Keep open")
            icon.name: "window-pin"
            checkable: true
            checked: root.weatherConfiguration.pinned
            onTriggered: Plasmoid.configuration.pinned = checked
        },
        PlasmaCore.Action {
            objectName: "choosePlaceAction"
            text: I18n.i18n("Choose location")
            icon.name: "mark-location"
            visible: placeSelection.places.length > 1
            onTriggered: {
                root.expanded = true;
                Qt.callLater(() => { if (root.fullRepresentationItem) root.fullRepresentationItem.chooseLocation(); });
            }
        },
        PlasmaCore.Action {
            objectName: "openForecastAction"
            text: I18n.i18n("Show meteogram")
            icon.name: "window-new"
            onTriggered: root.openMeteogram()
        }
    ]

    function openMeteogram() {
        if (ApplicationLauncher.openForecast(forecastModel.locationName, forecastModel.latitude,
            forecastModel.longitude, forecastModel.model)) {
            launchError = "";
            expanded = false;
        } else {
            launchError = I18n.i18n("Could not open Meteo. Install the desktop application and try again.");
            expanded = true;
        }
    }

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
        if (name.length) includePlace(placeSelection.placeKey(latitude, longitude));
        Plasmoid.configuration.locationName = name;
        Plasmoid.configuration.latitude = latitude;
        Plasmoid.configuration.longitude = longitude;
        if (name.length) Plasmoid.configuration.model = LocationRules.modelFor(latitude, longitude, Plasmoid.configuration.model);
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
    function includePlace(key) {
        const keys = Array.from(weatherConfiguration.visiblePlaces);
        if (keys.indexOf("*") < 0 && keys.indexOf(key) < 0) Plasmoid.configuration.visiblePlaces = keys.concat([key]);
    }
    function rememberSection(key, expanded) {
        const collapsed = Array.from(weatherConfiguration.collapsedCharts).filter(entry => entry !== key);
        if (!expanded) collapsed.push(key);
        if (JSON.stringify(collapsed) !== JSON.stringify(Array.from(weatherConfiguration.collapsedCharts)))
            Plasmoid.configuration.collapsedCharts = collapsed;
    }
    SavedPlaceSelection {
        id: placeSelection
        weather: forecastModel
        visibleKeys: root.weatherConfiguration.visiblePlaces
        orderKeys: root.weatherConfiguration.placeOrder
        onIncludePlaceRequested: key => root.includePlace(key)
        onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude)
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
        function chooseLocation() { if (forecastView.item) forecastView.item.chooseLocation(); }
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
            id: popupHeading
            leftPadding: Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.smallSpacing
            // extraControls precede the built-in menu and settings. Keep the pin last.
            readonly property PlasmaComponents.ToolButton pinControl: PlasmaComponents.ToolButton {
                id: pinButton
                parent: popupHeading.contentItem
                objectName: "pinPopupButton"
                icon.name: checked ? "window-unpin" : "window-pin"
                text: checked ? I18n.i18n("Unpin popup") : I18n.i18n("Keep open")
                display: AbstractButton.IconOnly
                checkable: true
                checked: root.weatherConfiguration.pinned
                onClicked: pinAction.trigger()
                PlasmaComponents.ToolTip { text: pinButton.text }
            }
            PlasmaComponents.ToolButton {
                id: refreshButton
                icon.name: "view-refresh"
                text: I18n.i18n("Refresh")
                display: AbstractButton.IconOnly
                enabled: refreshAction.enabled
                onClicked: refreshAction.trigger()
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
                    onClicked: root.openMeteogram()
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
                    id: forecastView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: fullView.charts ? widgetCharts : overview
                }
            }
        }
        Component {
            id: overview
            ForecastPopup {
                weather: forecastModel
                places: placeSelection.places
                showCharts: false
                onSectionToggled: (key, expanded) => root.rememberSection(key, expanded)
                onConfigureRequested: Plasmoid.internalAction("configure").trigger()
                onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude)
            }
        }
        Component {
            id: widgetCharts
            ForecastPopup {
                weather: forecastModel
                places: placeSelection.places
                profile: root.weatherConfiguration.chartProfile
                hours: root.weatherConfiguration.chartHours
                collapsedCharts: Array.from(root.weatherConfiguration.collapsedCharts)
                onSectionToggled: (key, expanded) => root.rememberSection(key, expanded)
                onConfigureRequested: Plasmoid.internalAction("configure").trigger()
                onLocationSelected: (name, latitude, longitude) => root.selectLocation(name, latitude, longitude)
            }
        }
    }
}
