pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ScrollView {
    id: root
    required property WeatherModel weather
    function number(value, decimals) { return value === null || value === undefined ? "—" : Number(value).toLocaleString(Qt.locale(), 'f', decimals); }
    implicitWidth: 0
    contentWidth: availableWidth
    clip: true
    GridLayout {
        id: dayGrid
        width: root.availableWidth
        uniformCellWidths: true
        columnSpacing: Kirigami.Units.smallSpacing
        rowSpacing: Kirigami.Units.smallSpacing
        columns: Math.max(1, Math.min(days.count, Math.floor(width / 160)))
        readonly property real cardWidth: (width - columnSpacing * (columns - 1)) / columns
        Repeater {
            id: days
            model: root.weather.dailyForecast
            delegate: Frame {
                id: dayCard
                objectName: "forecastDayCard"
                required property var modelData
                Layout.minimumWidth: 0
                Layout.preferredWidth: dayGrid.cardWidth
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 12
                contentItem: ColumnLayout {
                    spacing: 4
                    Label { Layout.fillWidth: true; Layout.preferredHeight: 28; text: dayCard.modelData.day; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap }
                    Kirigami.Icon { source: dayCard.modelData.icon; Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignHCenter; Accessible.name: dayCard.modelData.description }
                    Label { Layout.fillWidth: true; Layout.preferredHeight: 32; text: dayCard.modelData.description; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; opacity: 0.7; font.pixelSize: 11 }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Label { text: root.number(dayCard.modelData.maximum, 0) + "°"; font.pixelSize: 20; font.bold: true }
                        Label { text: root.number(dayCard.modelData.minimum, 0) + "°"; font.pixelSize: 16; opacity: 0.65 }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: I18n.i18n("Precipitation: ") + (dayCard.modelData.rainEstimated ? "≈ " : "") + root.number(dayCard.modelData.rain, 1) + " mm"
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 11
                        ToolTip.visible: rainHover.hovered
                        ToolTip.text: dayCard.modelData.rainEstimated
                            ? I18n.i18n("An interval crosses midnight. Its precipitation is shared between days in proportion to duration.")
                            : dayCard.modelData.rainComplete ? I18n.i18n("Total precipitation for this day.") : I18n.i18n("The forecast covers part of this day. Missing intervals are excluded.")
                        HoverHandler { id: rainHover }
                    }
                    Label { Layout.fillWidth: true; text: (dayCard.modelData.windEstimated ? I18n.i18n("Gusts: up to %1 %2") : I18n.i18n("Gusts: %1 %2")).arg(root.number(dayCard.modelData.wind, 0)).arg(root.weather.windUnit); wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11 }
                    Label { Layout.fillWidth: true; visible: dayCard.modelData.windEstimated; text: I18n.i18n("The gust period crosses midnight."); wrapMode: Text.WordWrap; font.pixelSize: 10; opacity: 0.7 }
                    Label { Layout.fillWidth: true; visible: dayCard.modelData.precipitationTypeUncertain; text: I18n.i18n("Precipitation type may vary within the period."); wrapMode: Text.WordWrap; font.pixelSize: 10; opacity: 0.7 }
                }
            }
        }
    }
}
