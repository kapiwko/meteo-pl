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
    signal hoursSelected(int hours)
    title: "Ustawienia"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(700, parent ? parent.width - 24 : 700)
    height: Math.min(720, parent ? parent.height - 24 : 720)
    padding: 0
    modal: true
    standardButtons: Dialog.Close
    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Close
        onRejected: root.reject()
        Button {
            text: "O aplikacji"
            icon.name: "help-about"
            DialogButtonBox.buttonRole: DialogButtonBox.HelpRole
            onClicked: aboutDialog.open()
        }
    }
    MeteoAbout { id: aboutDialog; weather: root.weather }
    component SettingsPage: ScrollView {
        id: page
        property alias pageContent: pagePane.contentItem
        contentWidth: availableWidth
        clip: true
        Pane {
            id: pagePane
            width: page.availableWidth
            padding: Kirigami.Units.largeSpacing
            background: null
        }
    }
    contentItem: ColumnLayout {
        spacing: 0
        TabBar {
            id: sections
            objectName: "settingsSections"
            Layout.fillWidth: true
            TabButton { text: "Prognoza"; icon.name: "weather-few-clouds"; icon.width: 16; icon.height: 16 }
            TabButton { text: "Profile"; icon.name: "view-statistics"; icon.width: 16; icon.height: 16 }
            TabButton { text: "Odczyt"; icon.name: "color-picker"; icon.width: 16; icon.height: 16 }
            TabButton { text: "Aplikacja"; icon.name: "configure"; icon.width: 16; icon.height: 16 }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sections.currentIndex
            SettingsPage {
                pageContent: ForecastSettings {
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
