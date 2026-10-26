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
    property int forecastView: 0
    property int newsView: 4
    readonly property int destination: forecastPanel.viewMode < 3 ? 0 : forecastPanel.viewMode === 3 ? 1 : 2
    readonly property alias panel: forecastPanel
    signal forecastSelected(string name, real latitude, real longitude, string model)
    padding: 0
    title: I18n.i18n("Meteo")
    function showDestination(index) { forecastPanel.viewMode = index === 0 ? forecastView : index === 1 ? 3 : newsView; }
    function openSettings() { navigationStack.push(settingsPage); }
    function chooseLocation() { navigationStack.push(locationPage); }
    header: ToolBar {
        MaterialStyle.Material.background: Kirigami.Theme.backgroundColor
        MaterialStyle.Material.foreground: Kirigami.Theme.textColor
        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing
            ToolButton {
                objectName: "mobilePlaceButton"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitHeight: 56
                icon.name: "mark-location"
                text: root.weather.locationName || I18n.i18n("Choose location")
                Accessible.name: I18n.i18n("My places · choose a forecast")
                onClicked: placesSheet.open()
            }
            ToolButton {
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
    footer: TabBar {
        objectName: "mobileNavigation"
        position: TabBar.Footer
        currentIndex: root.destination
        onCurrentIndexChanged: if (currentIndex >= 0) root.showDestination(currentIndex)
        MaterialStyle.Material.background: Kirigami.Theme.backgroundColor
        TabButton { text: I18n.i18n("Forecast"); icon.name: "weather-few-clouds"; icon.color: "transparent"; display: TabButton.TextUnderIcon; implicitHeight: 80 }
        TabButton { text: I18n.i18n("Maps"); icon.name: "globe"; icon.color: "transparent"; display: TabButton.TextUnderIcon; implicitHeight: 80 }
        TabButton { text: I18n.i18n("News"); icon.name: "view-list-text"; display: TabButton.TextUnderIcon; implicitHeight: 80 }
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
            TabButton { text: I18n.i18n("Meteogram") }
            TabButton { text: I18n.i18n("Hourly") }
            TabButton { text: I18n.i18n("Days") }
        }
        TabBar {
            objectName: "mobileNewsTabs"
            Layout.fillWidth: true
            visible: root.destination === 2
            currentIndex: root.newsView - 4
            onCurrentIndexChanged: if (visible && currentIndex >= 0) forecastPanel.viewMode = currentIndex + 4
            TabButton { text: I18n.i18n("Synoptic") }
            TabButton { text: I18n.i18n("Articles") }
        }
        ForecastPanel {
            id: forecastPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            weather: root.weather
            touchMode: true
            mobileLayout: true
            onConfigureRequested: root.chooseLocation()
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
            Repeater {
                model: UserPreferences.places
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    icon.name: "mark-location"
                    highlighted: LocationRules.samePlace(modelData.latitude, modelData.longitude, root.weather.latitude, root.weather.longitude)
                    onClicked: {
                        root.forecastSelected(modelData.name, modelData.latitude, modelData.longitude, root.weather.model);
                        placesSheet.close();
                    }
                }
            }
        }
        footer: Component { Button { text: I18n.i18n("Find or add a place"); icon.name: "list-add"; onClicked: { placesSheet.close(); root.chooseLocation(); } } }
    }
    MobileSheet {
        id: moreSheet
        objectName: "mobileMoreSheet"
        title: I18n.i18n("More options")
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            ItemDelegate { Layout.fillWidth: true; text: I18n.i18n("Location and model"); icon.name: "mark-location"; onClicked: { moreSheet.close(); root.chooseLocation(); } }
            ItemDelegate { Layout.fillWidth: true; text: I18n.i18n("Settings"); icon.name: "configure"; onClicked: { moreSheet.close(); root.openSettings(); } }
            ItemDelegate { Layout.fillWidth: true; text: I18n.i18n("About"); icon.name: "help-about"; icon.color: "transparent"; onClicked: { moreSheet.close(); about.open(); } }
        }
    }
    MeteoAbout { id: about; weather: root.weather }
    Component { id: settingsPage; MobileSettingsPage { weather: root.weather; navigationStack: root.navigationStack; forecastPanel: root.panel } }
    Component {
        id: locationPage
        MobileLocationPage {
            weather: root.weather
            navigationStack: root.navigationStack
            onForecastSelected: (name, latitude, longitude, model) => root.forecastSelected(name, latitude, longitude, model)
        }
    }
}
