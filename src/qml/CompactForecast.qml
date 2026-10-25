pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property bool showDays: true
    signal locationSelected(string name, real latitude, real longitude)
    readonly property var days: showDays ? weather.dailyForecast.slice(0, 7) : []
    spacing: Kirigami.Units.largeSpacing

    PlacePicker {
        Layout.fillWidth: true
        weather: root.weather
        onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude)
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing
        WeatherSymbol {
            id: weatherSymbol
            weather: root.weather
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Label { text: root.weather.temperatureText; font: Qt.font({family: Kirigami.Theme.defaultFont.family, pixelSize: 32, weight: Font.Bold}) }
            Label {
                Layout.fillWidth: true
                text: weatherSymbol.loading ? I18n.i18n("Downloading forecast…") : root.weather.summaryTime
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
            }
        }
        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing / 2
            visible: root.weather.hasData
            Label { text: I18n.i18n("Wind"); color: Kirigami.Theme.disabledTextColor; font: Kirigami.Theme.smallFont }
            Label { text: root.weather.windText; font.bold: true }
        }
    }
    Kirigami.InlineMessage { Layout.fillWidth: true; text: root.weather.error; visible: text.length > 0; type: Kirigami.MessageType.Warning }
    Kirigami.Separator { Layout.fillWidth: true; visible: root.days.length > 0 }
    GridLayout {
        id: forecastGrid
        Layout.fillWidth: true
        columns: Math.max(1, root.days.length)
        columnSpacing: Kirigami.Units.smallSpacing
        readonly property real cellWidth: Math.max(1, (width - columnSpacing * (columns - 1)) / columns)
        Repeater {
            model: root.days
            delegate: ColumnLayout {
                id: day
                required property var modelData
                Layout.minimumWidth: forecastGrid.cellWidth
                Layout.preferredWidth: forecastGrid.cellWidth
                Layout.maximumWidth: forecastGrid.cellWidth
                spacing: Kirigami.Units.smallSpacing
                Label { Layout.fillWidth: true; text: root.weather.formatTime(day.modelData.time, "ddd"); horizontalAlignment: Text.AlignHCenter; font.bold: true }
                Kirigami.Icon {
                    source: day.modelData.icon
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    Layout.alignment: Qt.AlignHCenter
                    Accessible.name: day.modelData.description
                    ToolTip.text: day.modelData.description
                    ToolTip.visible: dayHover.hovered
                    HoverHandler { id: dayHover }
                }
                Label { Layout.fillWidth: true; text: day.modelData.maximum === null ? "—" : Math.round(day.modelData.maximum) + "°"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                Label { Layout.fillWidth: true; text: day.modelData.minimum === null ? "—" : Math.round(day.modelData.minimum) + "°"; color: Kirigami.Theme.disabledTextColor; horizontalAlignment: Text.AlignHCenter }
                Label { Layout.fillWidth: true; text: day.modelData.rain === null ? "—" : (day.modelData.rainEstimated ? "≈ " : "") + Number(day.modelData.rain).toLocaleString(Qt.locale(), 'f', 1) + " mm"; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; font: Kirigami.Theme.smallFont; color: Kirigami.Theme.disabledTextColor }
            }
        }
    }
}
