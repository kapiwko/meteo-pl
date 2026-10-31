pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property bool active: true
    readonly property bool locating: weather.currentLocation.enabled && weather.currentLocation.busy
    readonly property bool naming: weather.currentLocation.enabled && weather.currentLocation.naming
    readonly property string positionError: weather.currentLocation.enabled ? weather.currentLocation.error : ""
    visible: active && (locating || naming || weather.busy || positionError.length > 0)
    spacing: Kirigami.Units.smallSpacing
    RowLayout {
        Layout.fillWidth: true
        visible: root.locating || root.naming || root.weather.busy
        BusyIndicator {
            objectName: "forecastActivityIndicator"
            running: visible
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
        }
        Label {
            objectName: "forecastActivityText"
            Layout.fillWidth: true
            text: root.locating ? I18n.i18n("Determining your location…") : root.naming ? I18n.i18n("Finding the place name…") : I18n.i18n("Downloading forecast…")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }
    }
    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: root.positionError.length > 0
        type: Kirigami.MessageType.Warning
        text: root.positionError
        actions: [Kirigami.Action { text: I18n.i18n("Try again"); icon.name: "view-refresh"; onTriggered: root.weather.currentLocation.retry() }]
    }
}
