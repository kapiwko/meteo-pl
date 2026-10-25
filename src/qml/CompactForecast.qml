pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    signal locationSelected(string name, real latitude, real longitude)
    readonly property var days: weather.dailyForecast.slice(0, 7)
    spacing: 10
    PlacePicker { Layout.fillWidth: true; weather: root.weather; centered: true; onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude) }
    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 24
        Label { text: root.weather.temperatureText; font.pointSize: 24; font.bold: true }
        WeatherSymbol { id: weatherSymbol; weather: root.weather; Layout.preferredWidth: 64; Layout.preferredHeight: 64 }
        ColumnLayout {
            Label { text: "Wiatr"; opacity: 0.65; font.pointSize: 9 }
            Label { text: root.weather.windText }
        }
    }
    Label { Layout.fillWidth: true; text: weatherSymbol.loading ? "Pobieranie prognozy…" : root.weather.summaryTime; horizontalAlignment: Text.AlignHCenter; opacity: 0.65; font.pointSize: 9 }
    Kirigami.InlineMessage { Layout.fillWidth: true; text: root.weather.error; visible: text.length > 0; type: Kirigami.MessageType.Warning }
    GridLayout {
        id: forecastGrid
        Layout.fillWidth: true
        columns: Math.max(1, root.days.length)
        columnSpacing: 4
        readonly property real cellWidth: Math.max(1, (width - columnSpacing * (columns - 1)) / columns)
        Repeater {
            model: root.days
            delegate: ColumnLayout {
                id: day
                required property var modelData
                Layout.minimumWidth: forecastGrid.cellWidth
                Layout.preferredWidth: forecastGrid.cellWidth
                Layout.maximumWidth: forecastGrid.cellWidth
                spacing: 5
                Label { Layout.fillWidth: true; text: root.weather.formatTime(day.modelData.time, "ddd"); horizontalAlignment: Text.AlignHCenter; font.pointSize: 9 }
                Kirigami.Icon { source: day.modelData.icon; Layout.preferredWidth: 36; Layout.preferredHeight: 36; Layout.alignment: Qt.AlignHCenter; Accessible.name: day.modelData.description; ToolTip.text: day.modelData.description; ToolTip.visible: dayHover.hovered; HoverHandler { id: dayHover } }
                Label { Layout.fillWidth: true; text: day.modelData.maximum === null ? "—" : Math.round(day.modelData.maximum) + "°"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                Label { Layout.fillWidth: true; text: day.modelData.minimum === null ? "—" : Math.round(day.modelData.minimum) + "°"; opacity: 0.65; horizontalAlignment: Text.AlignHCenter; font.pointSize: 9 }
                Label { Layout.fillWidth: true; text: day.modelData.rain > 0 ? Number(day.modelData.rain).toLocaleString(Qt.locale(), 'f', 1) + " mm" : "·"; horizontalAlignment: Text.AlignHCenter; font.pointSize: 8; opacity: 0.65 }
            }
        }
    }
    Item { Layout.fillHeight: true; Layout.minimumHeight: 0 }
    Label { Layout.fillWidth: true; text: root.weather.modelName + " · ICM UW / meteo.pl"; horizontalAlignment: Text.AlignHCenter; font.pointSize: 8; opacity: 0.6 }
}
