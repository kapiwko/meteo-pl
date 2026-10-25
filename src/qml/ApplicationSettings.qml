pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    objectName: "applicationSettings"
    spacing: Kirigami.Units.largeSpacing
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: "Wykresy"
        Layout.fillWidth: true
        contentItem: Kirigami.FormLayout {
            ComboBox {
                objectName: "rendererSetting"
                Kirigami.FormData.label: "Renderer:"
                Layout.fillWidth: true
                model: [{id: "gpu", name: "GPU · scena Qt Quick (domyślnie)"}, {id: "cpu", name: "CPU · Canvas"}]
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === UserPreferences.renderer)
                onActivated: index => UserPreferences.renderer = model[index].id
            }
            SpinBox {
                Kirigami.FormData.label: "Wysokość domyślna:"
                from: 64; to: 800; stepSize: 10
                value: UserPreferences.chartHeight
                textFromValue: value => value + " px"
                onValueModified: UserPreferences.chartHeight = value
            }
            Label {
                Layout.fillWidth: true
                text: "Profil i poszczególne wykresy mogą nadpisywać tę wysokość."
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
            CheckBox { text: "Cieniuj noc"; checked: UserPreferences.showNight; onToggled: UserPreferences.showNight = checked }
        }
    }
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: "Jednostki i czas"
        Layout.fillWidth: true
        contentItem: Kirigami.FormLayout {
            ComboBox {
                Kirigami.FormData.label: "Prędkość wiatru:"
                model: ["m/s", "km/h", "kt"]
                currentIndex: model.indexOf(UserPreferences.windUnit)
                onActivated: UserPreferences.windUnit = currentText
            }
            CheckBox { text: "Pokazuj czas UTC"; checked: UserPreferences.utc; onToggled: UserPreferences.utc = checked }
            Label { text: "Domyślnie używana jest strefa systemowa."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Kirigami.Theme.disabledTextColor }
        }
    }
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: "Konfiguracja"
        Layout.fillWidth: true
        contentItem: Kirigami.FormLayout {
            TextArea {
                objectName: "settingsFilePath"
                Kirigami.FormData.label: "Plik ustawień:"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 22
                text: UserPreferences.settingsFile
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                padding: 0
                background: null
            }
        }
    }
}
