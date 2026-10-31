pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "applicationSettings"
    property bool cacheCleared: false
    onVisibleChanged: if (visible) { cacheCleared = false; CacheStorage.refresh(); }
    Component.onCompleted: if (visible) CacheStorage.refresh()
    Connections { target: CacheStorage; function onCleared() { root.cacheCleared = true; } }
    spacing: Kirigami.Units.largeSpacing * 2
    PreferenceWriteNotice {}
    SettingsSection {
        title: I18n.i18n("Appearance")
        Layout.fillWidth: true
        contentItem: SettingsForm {
            id: appearanceForm
            twinFormLayouts: [chartsForm]
            ComboBox {
                objectName: "themeSetting"
                Kirigami.FormData.label: I18n.i18n("Color theme:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: appearanceForm.fieldWidth
                model: [{id: "automatic", name: I18n.i18n("Automatic (system)")}, {id: "light", name: I18n.i18n("Light")}, {id: "dark", name: I18n.i18n("Dark")}]
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === UserPreferences.theme)
                onActivated: index => UserPreferences.theme = model[index].id
            }
        }
    }
    SettingsSection {
        title: I18n.i18n("Charts")
        Layout.fillWidth: true
        contentItem: SettingsForm {
            id: chartsForm
            ComboBox {
                objectName: "rendererSetting"
                Kirigami.FormData.label: I18n.i18n("Renderer:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: chartsForm.fieldWidth
                model: [{id: "gpu", name: I18n.i18n("GPU acceleration (default)")}, {id: "cpu", name: I18n.i18n("CPU rendering")}]
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === UserPreferences.renderer)
                onActivated: index => UserPreferences.renderer = model[index].id
            }
            SpinBox {
                Kirigami.FormData.label: I18n.i18n("Default height:")
                from: 64; to: 800; stepSize: 10
                value: UserPreferences.chartHeight
                textFromValue: value => value + " px"
                onValueModified: UserPreferences.chartHeight = value
            }
            SettingsHint {
                Layout.fillWidth: true
                text: I18n.i18n("Profiles and individual charts can override this height.")
                wrapMode: Text.WordWrap
            }
            CheckBox { Layout.fillWidth: true; Layout.minimumWidth: 0; text: I18n.i18n("Shade nighttime"); checked: UserPreferences.showNight; onToggled: UserPreferences.showNight = checked }
        }
    }
    SettingsSection {
        title: I18n.i18n("Units and time")
        Layout.fillWidth: true
        contentItem: SettingsForm {
            id: unitsForm
            twinFormLayouts: [chartsForm]
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Wind speed:")
                objectName: "windUnitSetting"
                Layout.preferredWidth: unitsForm.fieldWidth
                Layout.minimumWidth: 0
                Layout.fillWidth: true
                model: ["m/s", "km/h", "kt"]
                currentIndex: model.indexOf(UserPreferences.windUnit)
                onActivated: UserPreferences.windUnit = currentText
            }
            CheckBox { Layout.fillWidth: true; Layout.minimumWidth: 0; text: I18n.i18n("Show UTC time"); checked: UserPreferences.utc; onToggled: UserPreferences.utc = checked }
            SettingsHint { text: I18n.i18n("The system time zone is used by default."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }
    SettingsSection {
        title: I18n.i18n("Cache")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            SettingsForm {
                twinFormLayouts: [chartsForm]
                Layout.fillWidth: true
                Label { Kirigami.FormData.label: I18n.i18n("Forecasts:"); text: CacheStorage.forecastSize }
                Label { Kirigami.FormData.label: I18n.i18n("Articles and commentary:"); text: CacheStorage.editorialSize }
                Label { Kirigami.FormData.label: I18n.i18n("Images and maps:"); text: CacheStorage.imageSize }
                Label { Kirigami.FormData.label: I18n.i18n("Search and models:"); text: CacheStorage.lookupSize }
                RowLayout {
                    Kirigami.FormData.label: I18n.i18n("Total on disk:")
                    Label { text: CacheStorage.sizeText; font.bold: true }
                    BusyIndicator { running: CacheStorage.busy; visible: running; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                }
                Button {
                    objectName: "clearCacheButton"
                    text: I18n.i18n("Clear cache")
                    icon.name: "edit-clear"
                    enabled: !CacheStorage.busy && CacheStorage.size > 0
                    onClicked: { root.cacheCleared = false; CacheStorage.clear(); }
                }
            }
            SettingsHint {
                Layout.fillWidth: true
                text: I18n.i18n("Data will be downloaded again when needed. Saved places and profiles will be kept.")
                wrapMode: Text.WordWrap
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.cacheCleared || CacheStorage.error.length > 0
                type: CacheStorage.error.length > 0 ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
                text: CacheStorage.error || I18n.i18n("The cache has been cleared.")
            }
        }
    }
}
