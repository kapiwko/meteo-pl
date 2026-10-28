pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "meteoSettingsDialog"
    required property WeatherModel weather
    property bool showTimeRange: false
    property real startTime: weather.firstTimestamp
    property real endTime: weather.lastTimestamp
    property alias section: sections.currentIndex
    signal modelSelected(string model)
    signal hoursSelected(int hours)
    title: I18n.i18n("Settings")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(860, parent ? parent.width - 24 : 860)
    height: Math.min(800, parent ? parent.height - 24 : 800)
    padding: 0
    modal: true
    standardButtons: Dialog.Close
    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Close
        onRejected: root.reject()
        Button {
            text: I18n.i18n("About")
            icon.name: "help-about"
            DialogButtonBox.buttonRole: DialogButtonBox.HelpRole
            onClicked: aboutDialog.open()
        }
    }
    MeteoAbout { id: aboutDialog; weather: root.weather }
    component SettingsPage: ScrollView {
        id: page
        property alias pageContent: pagePane.contentItem
        implicitWidth: 0
        contentWidth: availableWidth
        clip: true
        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        background: Pane { padding: 0 }
        Pane {
            id: pagePane
            width: Math.min(page.availableWidth, Kirigami.Units.gridUnit * 34)
            x: Math.max(0, (page.availableWidth - width) / 2)
            padding: Kirigami.Units.largeSpacing * 2
            background: null
        }
    }
    contentItem: ColumnLayout {
        spacing: 0
        Kirigami.NavigationTabBar {
            id: sections
            objectName: "settingsSections"
            Layout.fillWidth: true
            implicitWidth: 0
            maximumContentWidth: width
            currentIndex: 0
            spacing: Kirigami.Units.smallSpacing
            buttonWidth: Math.max(0, Math.floor((width - 3 * spacing) / 4))
            component SettingsTab: Kirigami.NavigationTabButton {
                required property int page
                display: root.width < 540 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                ButtonGroup.group: sections.tabGroup
                checked: page === sections.currentIndex
                onClicked: sections.currentIndex = page
                Layout.minimumWidth: sections.buttonWidth
                Layout.maximumWidth: sections.buttonWidth
                Layout.fillHeight: true
                icon.width: 20
                icon.height: 20
                Accessible.name: text
                ToolTip.text: text
                ToolTip.visible: hovered
            }
            SettingsTab { page: 0; text: I18n.i18n("Forecast"); icon.name: "weather-few-clouds" }
            SettingsTab { page: 1; text: I18n.i18n("Profiles"); icon.name: "view-statistics" }
            SettingsTab { page: 2; text: I18n.i18n("Readings"); icon.name: "color-picker" }
            SettingsTab { page: 3; text: I18n.i18n("Application"); icon.name: "configure" }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sections.currentIndex
            SettingsPage {
                pageContent: ForecastSettings {
                    onModelSelected: model => root.modelSelected(model)
                    weather: root.weather
                    showTimeRange: root.showTimeRange
                    startTime: root.startTime
                    endTime: root.endTime
                    onHoursSelected: hours => root.hoursSelected(hours)
                }
            }
            SettingsPage { pageContent: DisplaySettings { weather: root.weather } }
            SettingsPage { pageContent: ReadingSettings {} }
            SettingsPage { pageContent: ApplicationSettings {} }
        }
    }
}
