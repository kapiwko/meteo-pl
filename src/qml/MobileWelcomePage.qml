pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "mobileWelcomePage"
    property bool useCurrentLocation: true
    property bool rememberLocation: true
    property string selectedModel: "um4_60"
    property string selectedProfile: UserPreferences.profile
    property string selectedTheme: UserPreferences.theme
    property string selectedWindUnit: UserPreferences.windUnit
    signal completed(var options)
    signal skipped()
    title: I18n.i18n("Welcome to Meteo")
    padding: Kirigami.Units.largeSpacing * 2
    header: MobilePageHeader { title: root.title; showBack: false }
    ColumnLayout {
        width: Math.min(root.availableWidth, Kirigami.Units.gridUnit * 32)
        x: (root.availableWidth - width) / 2
        spacing: Kirigami.Units.largeSpacing * 2
        Label {
            Layout.fillWidth: true
            text: I18n.i18n("Set up your forecast. You can change these options later in Settings.")
            wrapMode: Text.WordWrap
        }
        SettingsSection {
            title: I18n.i18n("Location")
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                ButtonGroup { id: locationModes }
                RadioButton {
                    objectName: "welcomeCurrentLocation"
                    text: I18n.i18n("Current location")
                    checked: root.useCurrentLocation
                    ButtonGroup.group: locationModes
                    onClicked: root.useCurrentLocation = true
                }
                SettingsHint { text: I18n.i18n("Show the forecast for wherever you are. Location access is requested after you continue.") }
                CheckBox {
                    objectName: "welcomeRememberLocation"
                    text: I18n.i18n("Remember this choice")
                    visible: root.useCurrentLocation
                    checked: root.rememberLocation
                    onToggled: root.rememberLocation = checked
                }
                SettingsHint { visible: root.useCurrentLocation; text: I18n.i18n("Use Current location whenever Meteo starts.") }
                RadioButton {
                    objectName: "welcomeChoosePlace"
                    text: I18n.i18n("Choose location")
                    checked: !root.useCurrentLocation
                    ButtonGroup.group: locationModes
                    onClicked: root.useCurrentLocation = false
                }
                SettingsHint { text: I18n.i18n("Search for a town or choose a point on the map.") }
            }
        }
        SettingsSection {
            title: I18n.i18n("Forecast")
            contentItem: SettingsForm {
                id: forecastForm
                ComboBox {
                    objectName: "welcomeModel"
                    Kirigami.FormData.label: I18n.i18n("Forecast model:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: forecastForm.fieldWidth
                    model: [{id: "um4_60", name: I18n.i18n("UM4 · 60 hours")}, {id: "um4_120", name: I18n.i18n("UM4 · 120 hours")}, {id: "gfs", name: I18n.i18n("GFS · 240 hours")}]
                    textRole: "name"
                    currentIndex: model.findIndex(entry => entry.id === root.selectedModel)
                    onActivated: index => root.selectedModel = model[index].id
                }
                SettingsHint { text: I18n.i18n("UM4 covers Poland and Central Europe. GFS covers the whole world.") }
                ComboBox {
                    objectName: "welcomeProfile"
                    Kirigami.FormData.label: I18n.i18n("Chart profile:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: forecastForm.fieldWidth
                    model: UserPreferences.profiles
                    textRole: "name"
                    currentIndex: model.findIndex(profile => profile.id === root.selectedProfile)
                    onActivated: index => root.selectedProfile = model[index].id
                }
            }
        }
        SettingsSection {
            title: I18n.i18n("Appearance")
            contentItem: SettingsForm {
                id: appearanceForm
                ComboBox {
                    objectName: "welcomeTheme"
                    Kirigami.FormData.label: I18n.i18n("Color theme:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: appearanceForm.fieldWidth
                    model: [{id: "automatic", name: I18n.i18n("Automatic (system)")}, {id: "light", name: I18n.i18n("Light")}, {id: "dark", name: I18n.i18n("Dark")}]
                    textRole: "name"
                    currentIndex: model.findIndex(entry => entry.id === root.selectedTheme)
                    onActivated: index => root.selectedTheme = model[index].id
                }
                ComboBox {
                    objectName: "welcomeWindUnit"
                    Kirigami.FormData.label: I18n.i18n("Wind speed:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: appearanceForm.fieldWidth
                    model: ["m/s", "km/h", "kt"]
                    currentIndex: model.indexOf(root.selectedWindUnit)
                    onActivated: root.selectedWindUnit = currentText
                }
            }
        }
    }
    footer: Pane {
        padding: Kirigami.Units.largeSpacing
        leftPadding: Kirigami.Units.largeSpacing * 2
        rightPadding: Kirigami.Units.largeSpacing * 2
        contentItem: RowLayout {
            Button { text: I18n.i18n("Not now"); onClicked: root.skipped() }
            Button {
                objectName: "welcomeContinueButton"
                Layout.fillWidth: true
                text: root.useCurrentLocation ? I18n.i18n("Continue") : I18n.i18n("Choose location")
                highlighted: true
                onClicked: root.completed({useCurrentLocation: root.useCurrentLocation, rememberLocation: root.rememberLocation,
                    model: root.selectedModel, profile: root.selectedProfile, theme: root.selectedTheme, windUnit: root.selectedWindUnit})
            }
        }
    }
}
