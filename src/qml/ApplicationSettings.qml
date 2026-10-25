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
        title: "Pamięć podręczna"
        padding: Kirigami.Units.largeSpacing
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Label { Kirigami.FormData.label: "Prognozy:"; text: CacheStorage.forecastSize }
                Label { Kirigami.FormData.label: "Artykuły i komentarze:"; text: CacheStorage.editorialSize }
                Label { Kirigami.FormData.label: "Obrazy i mapy:"; text: CacheStorage.imageSize }
                Label { Kirigami.FormData.label: "Wyszukiwanie i modele:"; text: CacheStorage.lookupSize }
                RowLayout {
                    Kirigami.FormData.label: "Razem na dysku:"
                    Label { text: CacheStorage.sizeText; font.bold: true }
                    BusyIndicator { running: CacheStorage.busy; visible: running; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                }
            }
            Button {
                objectName: "clearCacheButton"
                text: "Wyczyść pamięć podręczną"
                icon.name: "edit-clear"
                enabled: !CacheStorage.busy && CacheStorage.size > 0
                onClicked: { root.cacheCleared = false; CacheStorage.clear(); }
            }
            Label {
                Layout.fillWidth: true
                text: "Dane zostaną pobrane ponownie, gdy będą potrzebne. Zapisane miejsca i profile pozostaną zachowane."
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.cacheCleared || CacheStorage.error.length > 0
                type: CacheStorage.error.length > 0 ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
                text: CacheStorage.error || "Pamięć podręczna została wyczyszczona."
            }
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
