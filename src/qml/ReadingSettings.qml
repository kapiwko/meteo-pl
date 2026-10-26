pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    objectName: "readingSettings"
    spacing: Kirigami.Units.largeSpacing
    PreferenceWriteNotice {}
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: I18n.i18n("Parameter identification")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            Kirigami.FormLayout {
                Layout.fillWidth: true
                ComboBox {
                    objectName: "probeModeSetting"
                    Kirigami.FormData.label: I18n.i18n("Show:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                    model: [{id: "chart", name: I18n.i18n("Parameters of the indicated chart")}, {id: "all", name: I18n.i18n("All parameters")}]
                    textRole: "name"
                    currentIndex: model.findIndex(entry => entry.id === UserPreferences.probeMode)
                    onActivated: index => UserPreferences.probeMode = model[index].id
                }
            }
            Label {
                Layout.fillWidth: true
                text: I18n.i18n("Enable the pipette in the chart status bar to read values under the cursor. Click to pin a reading.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: I18n.i18n("Chart legends")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            CheckBox {
                objectName: "defaultLegendSetting"
                text: I18n.i18n("Show legends by default")
                checked: UserPreferences.showLegend
                onToggled: UserPreferences.showLegend = checked
            }
            Label {
                Layout.fillWidth: true
                text: I18n.i18n("The information button next to a chart title toggles its legend. This option sets the initial state for all profiles.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }
}
