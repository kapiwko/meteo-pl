pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    objectName: "readingSettings"
    spacing: Kirigami.Units.largeSpacing
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: "Identyfikacja parametrów"
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            Kirigami.FormLayout {
                Layout.fillWidth: true
                ComboBox {
                    objectName: "probeModeSetting"
                    Kirigami.FormData.label: "Pokazuj:"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                    model: [{id: "chart", name: "Parametry wskazanego wykresu"}, {id: "all", name: "Wszystkie parametry"}]
                    textRole: "name"
                    currentIndex: model.findIndex(entry => entry.id === UserPreferences.probeMode)
                    onActivated: index => UserPreferences.probeMode = model[index].id
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Włącz próbnik na pasku wykresów, aby odczytywać parametry pod kursorem. Kliknięcie przypina odczyt."
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: "Legenda wykresów"
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            CheckBox {
                objectName: "defaultLegendSetting"
                text: "Domyślnie pokazuj legendę"
                checked: UserPreferences.showLegend
                onToggled: UserPreferences.showLegend = checked
            }
            Label {
                Layout.fillWidth: true
                text: "Przycisk informacji przy tytule wykresu przełącza legendę podczas oglądania. Ta opcja określa stan początkowy dla wszystkich profili."
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }
}
