pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property bool showTimeRange: false
    property real startTime: weather.firstTimestamp
    property real endTime: weather.lastTimestamp
    signal hoursSelected(int hours)
    spacing: Kirigami.Units.largeSpacing
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: "Bieżąca prognoza"
        Layout.fillWidth: true
        contentItem: Kirigami.FormLayout {
            ComboBox {
                Kirigami.FormData.label: "Profil wykresów:"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                Accessible.name: "Profil wykresów"
                model: UserPreferences.profiles
                textRole: "name"
                currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
                onActivated: index => UserPreferences.profile = model[index].id
            }
            ComboBox {
                objectName: "settingsTimeRange"
                Kirigami.FormData.label: "Zakres czasu:"
                visible: root.showTimeRange
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
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
            ComboBox {
                Kirigami.FormData.label: "Start modelu:"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
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
        }
    }
    Label {
        Layout.fillWidth: true
        text: "Zakres czasu i start modelu dotyczą bieżącej prognozy. Wybrany profil jest zapamiętywany."
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }
}
