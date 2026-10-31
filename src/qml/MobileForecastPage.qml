pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as MaterialStyle
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: root
    objectName: "mobileForecastPage"
    required property WeatherModel weather
    required property var navigationStack
    property bool offerSetup: true
    property int forecastView: 0
    property int newsView: 4
    readonly property bool shortLandscape: Window.window !== null && Window.window.width > Window.window.height && Window.window.height < 480
    readonly property int destination: forecastPanel.viewMode < 3 ? 0 : forecastPanel.viewMode === 3 ? 1 : 2
    readonly property alias panel: forecastPanel
    readonly property bool canNavigateBack: placesSheet.opened || moreSheet.opened || about.opened || destination !== 0 || forecastView !== 0
    function navigateBack() {
        if (placesSheet.opened) placesSheet.close();
        else if (moreSheet.opened) moreSheet.close();
        else if (about.opened) about.close();
        else if (forecastPanel.navigateBack()) return;
        else if (destination !== 0) showDestination(0);
        else if (forecastView !== 0) forecastPanel.viewMode = 0;
    }
    signal modelSelected(string model)
    signal forecastSelected(string name, real latitude, real longitude, string model)
    padding: 0
    title: I18n.i18n("Meteo")
    function showDestination(index) { forecastPanel.viewMode = index === 0 ? forecastView : index === 1 ? 3 : newsView; }
    function openSettings() { navigationStack.push(settingsPage); }
    Component.onCompleted: Qt.callLater(() => { if (root.offerSetup && !root.weather.locationName.length && !root.weather.currentLocation.enabled) root.beginSetup(); })
    function beginSetup() {
        if (UserPreferences.places.length) chooseLocation();
        else navigationStack.push(locationPage, {firstRun: true});
    }
    function chooseLocation() { navigationStack.push(placesPage); }
    function addLocation() { navigationStack.push(locationPage, {firstRun: UserPreferences.places.length === 0}); }
    header: ToolBar {
        MaterialStyle.Material.background: Kirigami.Theme.backgroundColor
        MaterialStyle.Material.foreground: Kirigami.Theme.textColor
        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing
            ToolButton {
                objectName: "mobilePlaceButton"
                visible: root.destination === 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitHeight: root.shortLandscape ? 48 : 56
                icon.name: root.weather.currentLocation.enabled ? "find-location" : "mark-location"
                text: root.weather.currentLocation.enabled && !root.weather.currentLocation.hasPosition ? I18n.i18n("Current location") : root.weather.locationName || I18n.i18n("Choose location")
                Accessible.name: I18n.i18n("My places · choose a forecast")
                onClicked: placesSheet.open()
            }
            Kirigami.Heading {
                objectName: "mobileDestinationTitle"
                visible: root.destination !== 0
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                level: 2
                text: root.destination === 1 ? I18n.i18n("Maps") : I18n.i18n("News")
                elide: Text.ElideRight
            }
            ToolButton {
                visible: root.destination === 0
                icon.name: "view-refresh"
                implicitWidth: 48; implicitHeight: 48
                enabled: !root.weather.busy
                Accessible.name: I18n.i18n("Refresh")
                onClicked: root.weather.refresh()
            }
            ToolButton {
                objectName: "mobileMoreButton"
                icon.name: "overflow-menu"
                implicitWidth: 48; implicitHeight: 48
                Accessible.name: I18n.i18n("More options")
                onClicked: moreSheet.open()
            }
        }
    }
    component MobileTab: TabButton {
        // Older qqc2-desktop-style provides a null contentItem while still
        // deriving its baseline from that item. Keep both native styles valid.
        baselineOffset: contentItem ? contentItem.y + contentItem.baselineOffset : height / 2
    }
    component SectionTab: MobileTab {
        display: TabButton.TextBesideIcon
        icon.width: 20
        icon.height: 20
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
    }
    footer: TabBar {
        objectName: "mobileNavigation"
        position: TabBar.Footer
        currentIndex: root.destination
        onCurrentIndexChanged: if (currentIndex >= 0) root.showDestination(currentIndex)
        MaterialStyle.Material.background: Kirigami.Theme.backgroundColor
        MobileTab { text: I18n.i18n("Forecast"); icon.name: "weather-few-clouds"; icon.color: "transparent"; display: root.shortLandscape ? TabButton.TextBesideIcon : TabButton.TextUnderIcon; implicitHeight: root.shortLandscape ? 48 : 80 }
        MobileTab { text: I18n.i18n("Maps"); icon.name: "globe"; icon.color: "transparent"; display: root.shortLandscape ? TabButton.TextBesideIcon : TabButton.TextUnderIcon; implicitHeight: root.shortLandscape ? 48 : 80 }
        MobileTab { text: I18n.i18n("News"); icon.name: "view-list-text"; display: root.shortLandscape ? TabButton.TextBesideIcon : TabButton.TextUnderIcon; implicitHeight: root.shortLandscape ? 48 : 80 }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        TabBar {
            objectName: "mobileForecastTabs"
            Layout.fillWidth: true
            visible: root.destination === 0
            currentIndex: root.forecastView
            onCurrentIndexChanged: if (visible && currentIndex >= 0) forecastPanel.viewMode = currentIndex
            SectionTab { text: I18n.i18n("Meteogram"); icon.name: "office-chart-line" }
            SectionTab { text: I18n.i18n("Hourly"); icon.name: "chronometer" }
            SectionTab { text: I18n.i18n("Days"); icon.name: "view-calendar" }
        }
        TabBar {
            objectName: "mobileNewsTabs"
            Layout.fillWidth: true
            visible: root.destination === 2
            currentIndex: root.newsView - 4
            onCurrentIndexChanged: if (visible && currentIndex >= 0) forecastPanel.viewMode = currentIndex + 4
            SectionTab { text: I18n.i18n("Synoptic"); icon.name: "dialog-messages" }
            SectionTab { text: I18n.i18n("Articles"); icon.name: "view-list-text" }
        }
        ForecastPanel {
            id: forecastPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            weather: root.weather
            touchMode: true
            mobileLayout: true
            onConfigureRequested: root.chooseLocation()
            onAddLocationRequested: root.addLocation()
            onModelSelected: model => root.modelSelected(model)
            onSettingsRequested: root.openSettings()
            onLocationSelected: (name, latitude, longitude) => root.forecastSelected(name, latitude, longitude, root.weather.model)
            onViewModeChanged: {
                if (viewMode < 3) root.forecastView = viewMode;
                else if (viewMode >= 4) root.newsView = viewMode;
            }
        }
    }
    MobileSheet {
        id: placesSheet
        objectName: "mobilePlacesSheet"
        title: I18n.i18n("My places")
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            ItemDelegate {
                objectName: "mobileCurrentLocationPlace"
                Layout.fillWidth: true
                text: I18n.i18n("Current location")
                icon.name: "find-location"
                highlighted: root.weather.currentLocation.enabled
                onClicked: { root.weather.currentLocation.enabled = true; placesSheet.close(); }
            }
            Repeater {
                model: UserPreferences.places
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    icon.name: "mark-location"
                    highlighted: !root.weather.currentLocation.enabled && LocationRules.samePlace(modelData.latitude, modelData.longitude, root.weather.latitude, root.weather.longitude)
                    onClicked: {
                        root.forecastSelected(modelData.name, modelData.latitude, modelData.longitude, root.weather.model);
                        placesSheet.close();
                    }
                }
            }
        }
        footer: Component { Button { text: I18n.i18n("Add location"); icon.name: "list-add"; highlighted: true; onClicked: { placesSheet.close(); root.addLocation(); } } }
    }
    MobileSheet {
        id: moreSheet
        objectName: "mobileMoreSheet"
        title: I18n.i18n("More options")
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            ItemDelegate { visible: root.destination === 0; Layout.fillWidth: true; text: I18n.i18n("My places"); icon.name: "mark-location"; onClicked: { moreSheet.close(); root.chooseLocation(); } }
            ItemDelegate { Layout.fillWidth: true; text: I18n.i18n("Settings"); icon.name: "configure"; onClicked: { moreSheet.close(); root.openSettings(); } }
            ItemDelegate { Layout.fillWidth: true; text: I18n.i18n("About"); icon.name: "help-about"; icon.color: "transparent"; onClicked: { moreSheet.close(); about.open(); } }
        }
    }
    MeteoAbout { id: about; weather: root.weather }
    Component { id: settingsPage; MobileSettingsPage { weather: root.weather; navigationStack: root.navigationStack; forecastPanel: root.panel } }
    Component {
        id: locationPage
        MobileLocationPage {
            navigationStack: root.navigationStack
            onPlaceSaved: (place, added) => {
                if (added) root.forecastSelected(place.name, place.latitude, place.longitude, root.weather.model);
                root.navigationStack.pop();
            }
        }
    }
    Component {
        id: placesPage
        MobilePlacesPage {
            weather: root.weather
            navigationStack: root.navigationStack
            onLocationSelected: (name, latitude, longitude) => root.forecastSelected(name, latitude, longitude, root.weather.model)
        }
    }
}
