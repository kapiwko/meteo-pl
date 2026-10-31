pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property bool touchMode: Kirigami.Settings.isMobile
    objectName: "readingSettings"
    spacing: Kirigami.Units.largeSpacing * 2
    PreferenceWriteNotice {}
    SettingsSection {
        title: I18n.i18n("Parameter identification")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            SettingsForm {
                id: probeForm
                Layout.fillWidth: true
                ComboBox {
                    objectName: "probeModeSetting"
                    Kirigami.FormData.label: I18n.i18n("Show:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: probeForm.fieldWidth
                    model: [{id: "chart", name: I18n.i18n("Parameters of the indicated chart")}, {id: "all", name: I18n.i18n("All parameters")}]
                    textRole: "name"
                    currentIndex: model.findIndex(entry => entry.id === UserPreferences.probeMode)
                    onActivated: index => UserPreferences.probeMode = model[index].id
                }
            }
            SettingsHint {
                Layout.fillWidth: true
                text: root.touchMode
                    ? I18n.i18n("Enable the pipette in the header, then tap a chart to read its values.")
                    : I18n.i18n("Enable the pipette in the chart status bar to read values under the cursor. Click to pin a reading.")
                wrapMode: Text.WordWrap
            }
        }
    }
    SettingsSection {
        title: I18n.i18n("Chart legends")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            CheckBox {
                objectName: "defaultLegendSetting"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: I18n.i18n("Show legends by default")
                checked: UserPreferences.showLegend
                onToggled: UserPreferences.showLegend = checked
            }
            SettingsHint {
                Layout.fillWidth: true
                text: I18n.i18n("The information button next to a chart title toggles its legend. This option sets the initial state for all profiles.")
                wrapMode: Text.WordWrap
            }
        }
    }
}
