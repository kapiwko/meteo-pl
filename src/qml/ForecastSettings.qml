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
    signal modelSelected(string model)
    signal hoursSelected(int hours)
    signal rangeSelected(real first, real last)
    function intervalIndex() {
        const hours = (endTime - startTime) / 3600;
        if (Math.abs(startTime - weather.firstTimestamp) < 1 && Math.abs(endTime - weather.lastTimestamp) < 1) return 0;
        const index = [24,48,72].findIndex(value => Math.abs(value - hours) < 0.01);
        return index < 0 ? 4 : index + 1;
    }
    TimeRangeDialog {
        id: intervalDialog
        minimum: root.weather.firstTimestamp
        maximum: root.weather.lastTimestamp
        startTime: root.startTime
        endTime: root.endTime
        onRangeSelected: (first, last) => root.rangeSelected(first, last)
        onClosed: intervalSelector.currentIndex = Qt.binding(() => root.intervalIndex())
    }
    spacing: Kirigami.Units.largeSpacing * 2
    PreferenceWriteNotice {}
    SettingsSection {
        title: I18n.i18n("Current forecast")
        Layout.fillWidth: true
        contentItem: SettingsForm {
            id: forecastForm
            ComboBox {
                objectName: "forecastModelSelector"
                Kirigami.FormData.label: I18n.i18n("Forecast model:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: forecastForm.fieldWidth
                model: [{id: "um4_60", name: I18n.i18n("UM4 · 60 hours")}, {id: "um4_120", name: I18n.i18n("UM4 · 120 hours")}, {id: "gfs", name: I18n.i18n("GFS · 240 hours")}]
                textRole: "name"
                currentIndex: model.findIndex(entry => entry.id === root.weather.model)
                delegate: ItemDelegate {
                    required property int index
                    required property var modelData
                    width: parent ? parent.width : implicitWidth
                    text: modelData.name
                    enabled: LocationRules.valid(root.weather.latitude, root.weather.longitude, modelData.id)
                }
                onActivated: index => root.modelSelected(model[index].id)
            }
            SettingsHint {
                Layout.fillWidth: true
                text: I18n.i18n("UM4 covers Poland and Central Europe. GFS covers the whole world.")
                wrapMode: Text.WordWrap
            }
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Chart profile:")
                objectName: "forecastProfileSelector"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: forecastForm.fieldWidth
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
                Layout.preferredWidth: forecastForm.fieldWidth
                Accessible.name: I18n.i18n("Chart time interval")
                model: [I18n.i18n("Entire forecast"), I18n.i18n("24 hours"), I18n.i18n("48 hours"), I18n.i18n("72 hours"), I18n.i18n("Custom interval")]
                id: intervalSelector
                enabled: root.weather.hasData
                currentIndex: root.intervalIndex()
                onActivated: index => {
                    if (index < 4) root.hoursSelected([0,24,48,72][index]);
                    else intervalDialog.open();
                }
            }
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Model run:")
                objectName: "forecastRunSelector"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: forecastForm.fieldWidth
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
    SettingsHint {
        Layout.fillWidth: true
        text: I18n.i18n("The time interval and model run apply to this view. The selected profile is remembered.")
        wrapMode: Text.WordWrap
    }
}
