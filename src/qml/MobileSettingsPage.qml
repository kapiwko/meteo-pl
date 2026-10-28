pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "mobileSettingsPage"
    required property WeatherModel weather
    required property var navigationStack
    required property ForecastPanel forecastPanel
    readonly property var sections: [
        {title: I18n.i18n("Forecast"), description: I18n.i18n("Profile, time interval and model run"), icon: "weather-few-clouds"},
        {title: I18n.i18n("Profiles"), description: I18n.i18n("Charts, series and appearance"), icon: "view-statistics"},
        {title: I18n.i18n("Readings"), description: I18n.i18n("Parameter identification and legends"), icon: "color-picker"},
        {title: I18n.i18n("Application"), description: I18n.i18n("Renderer, units and cache"), icon: "configure"}
    ]
    title: I18n.i18n("Settings")
    padding: 0
    header: MobilePageHeader { title: root.title; onBackRequested: root.navigationStack.pop() }
    ColumnLayout {
        width: root.availableWidth
        spacing: 0
        Repeater {
            model: root.sections
            delegate: ItemDelegate {
                id: entry
                required property var modelData
                required property int index
                objectName: "mobileSettingsSection-" + index
                Layout.fillWidth: true
                leftPadding: Kirigami.Units.largeSpacing * 2
                rightPadding: Kirigami.Units.largeSpacing * 2
                implicitHeight: 76
                Accessible.name: modelData.title + ". " + modelData.description
                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon { source: entry.modelData.icon; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        Label { text: entry.modelData.title; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: entry.modelData.description; Layout.fillWidth: true; wrapMode: Text.WordWrap; font: Kirigami.Theme.smallFont; color: Kirigami.Theme.disabledTextColor }
                    }
                    Kirigami.Icon { source: "go-next"; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
                }
                onClicked: root.navigationStack.push(sectionPage, {section: index})
            }
        }
        ItemDelegate {
            Layout.fillWidth: true
            leftPadding: Kirigami.Units.largeSpacing * 2
            rightPadding: Kirigami.Units.largeSpacing * 2
            text: I18n.i18n("About")
            icon.name: "help-about"
            icon.color: "transparent"
            onClicked: about.open()
        }
    }
    MeteoAbout { id: about; weather: root.weather }
    Component {
        id: sectionPage
        Kirigami.ScrollablePage {
            id: sectionView
            required property int section
            objectName: "mobileSettingsDetail-" + section
            title: root.sections[section].title
            padding: Kirigami.Units.largeSpacing * 2
            header: MobilePageHeader { title: sectionView.title; onBackRequested: root.navigationStack.pop() }
            Loader {
                width: sectionView.availableWidth
                sourceComponent: [forecastSettings, profileSettings, readingSettings, applicationSettings][sectionView.section]
            }
        }
    }
    Component {
        id: forecastSettings
        ForecastSettings {
            weather: root.weather
            onModelSelected: model => root.forecastPanel.modelSelected(model)
            showTimeRange: true
            startTime: root.forecastPanel.viewportStart
            endTime: root.forecastPanel.viewportEnd
            onHoursSelected: hours => root.forecastPanel.setHours(hours)
            onRangeSelected: (first, last) => root.forecastPanel.setRange(first, last)
        }
    }
    Component { id: profileSettings; DisplaySettings { weather: root.weather } }
    Component { id: readingSettings; ReadingSettings {} }
    Component { id: applicationSettings; ApplicationSettings {} }
}
