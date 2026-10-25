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
        title: I18n.i18n("Current forecast")
        Layout.fillWidth: true
        contentItem: Kirigami.FormLayout {
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Chart profile:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                Accessible.name: I18n.i18n("Chart profile")
                model: UserPreferences.profiles
                textRole: "name"
                currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
                onActivated: index => UserPreferences.profile = model[index].id
            }
            ComboBox {
                objectName: "settingsTimeRange"
                Kirigami.FormData.label: I18n.i18n("Time interval:")
                visible: root.showTimeRange
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                Accessible.name: I18n.i18n("Chart time interval")
                model: [I18n.i18n("Entire forecast"), I18n.i18n("24 hours"), I18n.i18n("48 hours"), I18n.i18n("72 hours"), I18n.i18n("Custom interval")]
                currentIndex: {
                    const hours = (root.endTime - root.startTime) / 3600;
                    if (Math.abs(root.startTime - root.weather.firstTimestamp) < 1 && Math.abs(root.endTime - root.weather.lastTimestamp) < 1) return 0;
                    const index = [24,48,72].findIndex(value => Math.abs(value - hours) < 0.01);
                    return index < 0 ? 4 : index + 1;
                }
                onActivated: index => { if (index < 4) root.hoursSelected([0,24,48,72][index]); }
            }
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Model run:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                Accessible.name: I18n.i18n("Model run, including archived forecasts")
                model: root.weather.availableRuns.length ? root.weather.availableRuns : [{timestamp: 0, label: I18n.i18n("Latest available")}]
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
        text: I18n.i18n("The time interval and model run apply to this view. The selected profile is remembered.")
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }
}
