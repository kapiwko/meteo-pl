pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    objectName: "applicationSettings"
    spacing: 12
    Label { text: "Ogólna wysokość wykresów"; font.bold: true }
    SpinBox { from: 64; to: 800; stepSize: 10; value: UserPreferences.chartHeight; textFromValue: value => value + " px"; onValueModified: UserPreferences.chartHeight = value }
    Label { Layout.fillWidth: true; text: "Własna wysokość wykresu ma pierwszeństwo przed wysokością profilu, a profil przed ogólną. Pasek kierunku wiatru zachowuje proporcjonalnie mniejszą wysokość, jeśli nie ma własnej wartości."; wrapMode: Text.WordWrap; opacity: 0.7 }
    Label { text: "Rysowanie wykresów"; font.bold: true }
    ComboBox {
        Layout.fillWidth: true
        model: [{id:"gpu",name:"GPU · scena Qt Quick (domyślnie)"}, {id:"cpu",name:"CPU · Canvas"}]
        textRole: "name"
        currentIndex: model.findIndex(item => item.id === UserPreferences.renderer)
        onActivated: index => UserPreferences.renderer = model[index].id
    }
    Label { Layout.fillWidth: true; text: "Zmiana działa od razu dla wszystkich profili. Gdy Qt Quick działa bez akceleracji, aplikacja użyje CPU."; wrapMode: Text.WordWrap; opacity: 0.7 }
    Label { text: "Jednostka wiatru"; font.bold: true }
    ComboBox { Layout.fillWidth: true; model: ["m/s", "km/h", "kt"]; currentIndex: model.indexOf(UserPreferences.windUnit); onActivated: UserPreferences.windUnit = currentText }
    CheckBox { text: "Czas UTC"; checked: UserPreferences.utc; onToggled: UserPreferences.utc = checked }
    CheckBox { text: "Cieniuj noc dla wybranej lokalizacji"; checked: UserPreferences.showNight; onToggled: UserPreferences.showNight = checked }
    TextEdit {
        Layout.fillWidth: true
        text: "Plik konfiguracji: " + UserPreferences.settingsFile
        readOnly: true
        selectByMouse: true
        wrapMode: TextEdit.WrapAnywhere
        font.pixelSize: 11
        color: Kirigami.Theme.textColor
        opacity: 0.7
    }
    Label { Layout.fillWidth: true; text: "Legenda ukrywa serie bez zmiany skali. Ctrl + kółko myszy przybliża czas. Włączony próbnik pozwala przypiąć odczyt kliknięciem."; wrapMode: Text.WordWrap; opacity: 0.7 }
}
