pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "welcomeWizard"
    property bool mobileLayout: Kirigami.Settings.isMobile
    property bool showHeader: true
    property int step: 0
    readonly property int stepCount: 4
    readonly property bool canNavigateBack: step > 0
    property bool useCurrentLocation: true
    property string selectedModel: "um4_60"
    property string selectedProfile: UserPreferences.profile
    property string selectedTheme: UserPreferences.theme
    property string selectedWindUnit: UserPreferences.windUnit
    signal completed(var options)
    signal skipped()
    function navigateBack() { if (canNavigateBack) --step; }
    function advance() {
        if (step < stepCount - 1) { ++step; return; }
        completed({useCurrentLocation: useCurrentLocation, model: selectedModel, profile: selectedProfile,
            theme: selectedTheme, windUnit: selectedWindUnit});
    }
    onStepChanged: { if (flickable) flickable.contentY = 0; }
    title: [I18n.i18n("Meteo"), I18n.i18n("Location"), I18n.i18n("Forecast"), I18n.i18n("Appearance")][step]
    padding: Kirigami.Units.largeSpacing * 2
    header: MobilePageHeader { title: root.title; showBack: false; visible: root.showHeader }
    ColumnLayout {
        width: Math.min(root.availableWidth, Kirigami.Units.gridUnit * 32)
        x: (root.availableWidth - width) / 2
        spacing: Kirigami.Units.largeSpacing * 2
        ColumnLayout {
            visible: root.step === 0
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing * 2
            Kirigami.Icon {
                objectName: "welcomeLogo"
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Kirigami.Units.largeSpacing * 2
                Layout.preferredWidth: Kirigami.Units.iconSizes.enormous
                Layout.preferredHeight: Kirigami.Units.iconSizes.enormous
                source: "qrc:/icons/meteo.svg"
                Accessible.ignored: true
            }
            Kirigami.Heading {
                Layout.fillWidth: true
                level: 1
                text: I18n.i18n("Welcome to Meteo")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                text: I18n.i18n("Weather forecasts, meteograms and maps from meteo.pl.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            SettingsHint {
                text: I18n.i18n("Set up your forecast. You can change these options later in Settings.")
                horizontalAlignment: Text.AlignHCenter
            }
        }
        ColumnLayout {
            visible: root.step === 1
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing * 2
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                ButtonGroup { id: locationModes }
                RadioButton {
                    objectName: "welcomeCurrentLocation"
                    text: I18n.i18n("Current location")
                    checked: root.useCurrentLocation
                    ButtonGroup.group: locationModes
                    onClicked: root.useCurrentLocation = true
                }
                SettingsHint { text: I18n.i18n("Follow your position when you use Meteo. Location access is requested when you finish setup.") }
                RadioButton {
                    objectName: "welcomeChoosePlace"
                    text: I18n.i18n("Choose location")
                    checked: !root.useCurrentLocation
                    ButtonGroup.group: locationModes
                    onClicked: root.useCurrentLocation = false
                }
                SettingsHint { text: I18n.i18n("Search for a town or choose a point on the map.") }
            }
            Kirigami.InlineMessage {
                objectName: "welcomeMultiplePlacesHint"
                Layout.fillWidth: true
                visible: true
                type: Kirigami.MessageType.Information
                text: I18n.i18n("You can save multiple places and switch between them using the location name above the forecast.")
            }
        }
        SettingsForm {
            id: forecastForm
            visible: root.step === 2
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
        SettingsForm {
            id: appearanceForm
            visible: root.step === 3
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
    footer: Pane {
        padding: Kirigami.Units.largeSpacing
        leftPadding: Kirigami.Units.largeSpacing * 2
        rightPadding: Kirigami.Units.largeSpacing * 2
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.i18n("Step %1 of %2").arg(root.step + 1).arg(root.stepCount)
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true; visible: !root.mobileLayout }
                Button {
                    objectName: "welcomeBackButton"
                    text: root.canNavigateBack ? I18n.i18n("Back") : I18n.i18n("Not now")
                    onClicked: root.canNavigateBack ? root.navigateBack() : root.skipped()
                }
                Button {
                    objectName: "welcomeContinueButton"
                    Layout.fillWidth: root.mobileLayout
                    text: root.step < root.stepCount - 1 ? I18n.i18n("Next")
                        : root.useCurrentLocation ? I18n.i18n("Show forecast") : I18n.i18n("Choose location")
                    highlighted: true
                    onClicked: root.advance()
                }
            }
        }
    }
}
