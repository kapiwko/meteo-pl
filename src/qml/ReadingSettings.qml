pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    objectName: "readingSettings"
    spacing: 12
    CheckBox { text: "Domyślnie pokazuj legendę"; checked: UserPreferences.showLegend; onToggled: UserPreferences.showLegend = checked }
    Label { text: "Parametry w trybie identyfikacji"; font.bold: true }
    ComboBox {
        objectName: "probeModeSetting"
        Layout.fillWidth: true
        model: [{id: "chart", name: "Parametry wskazanego wykresu"}, {id: "all", name: "Wszystkie parametry"}]
        textRole: "name"
        currentIndex: model.findIndex(entry => entry.id === UserPreferences.probeMode)
        onActivated: index => UserPreferences.probeMode = model[index].id
    }
    Label { Layout.fillWidth: true; text: "Włącz próbnik na pasku pod wykresami, aby odczytywać parametry pod kursorem. Kliknięcie w tym trybie przypina odczyt. Wszystkie parametry mają karty kategorii i dwie kolumny przy szerokim oknie."; wrapMode: Text.WordWrap; opacity: 0.7 }
    Label { Layout.fillWidth: true; text: "Przycisk informacji obok tytułu wykresu przełącza legendę tylko podczas bieżącego oglądania. Tutaj wybierasz jej domyślną widoczność dla wszystkich profili."; wrapMode: Text.WordWrap; opacity: 0.7 }
}
