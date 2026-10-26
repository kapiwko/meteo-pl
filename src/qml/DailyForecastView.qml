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
        columns: Math.max(1, Math.min(days.count, Math.floor(width / 160)))
        readonly property real cardWidth: (width - columnSpacing * (columns - 1)) / columns
        Repeater {
            id: days
            model: root.weather.dailyForecast
            delegate: Frame {
                id: dayCard
                objectName: "forecastDayCard"
                required property var modelData
                Layout.minimumWidth: dayGrid.cardWidth
                Layout.preferredWidth: dayGrid.cardWidth
                Layout.maximumWidth: dayGrid.cardWidth
                Layout.fillHeight: true
                padding: 12
                ColumnLayout {
                    anchors.fill: parent
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
                    Label { Layout.fillWidth: true; text: I18n.i18n("Gusts: ") + root.number(dayCard.modelData.wind, 0) + " " + root.weather.windUnit; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11 }
                }
            }
        }
    }
}
