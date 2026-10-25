pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "meteoSettingsDialog"
    required property WeatherModel weather
    property bool showTimeRange: false
    property real startTime: weather.firstTimestamp
    property real endTime: weather.lastTimestamp
    property alias section: sections.currentIndex
    signal hoursSelected(int hours)
    title: "Ustawienia"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(650, parent ? parent.width - 32 : 650)
    height: Math.min(section === 0 ? 480 : 740, parent ? parent.height - 32 : 740)
    padding: 16
    modal: true
    standardButtons: Dialog.Close
    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Close
        onRejected: root.reject()
        Button {
            text: "O aplikacji"
            icon.name: "help-about"
            DialogButtonBox.buttonRole: DialogButtonBox.HelpRole
            onClicked: aboutDialog.open()
        }
    }
    MeteoAbout { id: aboutDialog; weather: root.weather }
    contentItem: ColumnLayout {
        spacing: 12
        TabBar {
            id: sections
            Layout.fillWidth: true
            TabButton { text: "Prognoza"; icon.name: "weather-few-clouds"; icon.width: 16; icon.height: 16 }
            TabButton { text: "Wykresy i aplikacja"; icon.name: "configure"; icon.width: 16; icon.height: 16 }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sections.currentIndex
            ScrollView {
                id: forecastScroll
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: forecastScroll.availableWidth
                    spacing: 12
                    Label { text: "Profil wykresów"; font.bold: true }
                    ComboBox {
                        Layout.fillWidth: true
                        Accessible.name: "Profil wykresów"
                        model: UserPreferences.profiles
                        textRole: "name"
                        currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
                        onActivated: index => UserPreferences.profile = model[index].id
                    }
                    Label { text: "Zakres czasu"; font.bold: true; visible: root.showTimeRange }
                    ComboBox {
                        objectName: "settingsTimeRange"
                        visible: root.showTimeRange
                        Layout.fillWidth: true
                        Accessible.name: "Zakres czasu na wykresach"
                        model: ["Cała prognoza", "24 godziny", "48 godzin", "72 godziny", "Własny zakres"]
                        currentIndex: {
                            const hours = (root.endTime - root.startTime) / 3600;
                            if (Math.abs(root.startTime - root.weather.firstTimestamp) < 1 && Math.abs(root.endTime - root.weather.lastTimestamp) < 1) return 0;
                            const index = [24,48,72].findIndex(value => Math.abs(value - hours) < 0.01);
                            return index < 0 ? 4 : index + 1;
                        }
                        onActivated: index => { if (index < 4) root.hoursSelected([0,24,48,72][index]); }
                    }
                    Label { text: "Start modelu"; font.bold: true }
                    ComboBox {
                        Layout.fillWidth: true
                        Accessible.name: "Start obliczeń modelu, także archiwalny"
                        model: root.weather.availableRuns.length ? root.weather.availableRuns : [{timestamp: 0, label: "Najnowszy dostępny"}]
                        textRole: "label"
                        onDownChanged: if (down) root.weather.loadRuns()
                        currentIndex: {
                            const index = root.weather.availableRuns.findIndex(entry => entry.timestamp === root.weather.requestedRun);
                            return Math.max(0, index);
                        }
                        onActivated: index => {
                            if (!root.weather.availableRuns.length) root.weather.refresh();
                            else root.weather.requestedRun = root.weather.availableRuns[index].timestamp;
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Zakres czasu i start modelu dotyczą bieżącej prognozy. Wybrany profil jest zapamiętywany. Profile i ustawienia aplikacji edytujesz w zakładce „Wykresy i aplikacja”."
                        opacity: 0.7
                        wrapMode: Text.WordWrap
                    }
                }
            }
            ScrollView {
                id: displayScroll
                contentWidth: availableWidth
                clip: true
                DisplaySettings { width: displayScroll.availableWidth; weather: root.weather }
            }
        }
    }
}
