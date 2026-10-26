pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

ScrollView {
    id: root
    objectName: "hourlyForecastTable"
    required property WeatherModel weather
    signal readingRequested(real timestamp, point position)
    clip: true
    contentWidth: Math.max(availableWidth, 1040)
    readonly property var columns: [
        {key: "airtmp_point", title: I18n.i18n("Temp.")},
        {key: "wchill_point", title: I18n.i18n("Feels like")},
        {key: "pcpttl_aver", title: I18n.i18n("Precipitation")},
        {key: "pcpttlprob_point", title: I18n.i18n("Chance")},
        {key: "wind10_sd_true_prev_point", title: I18n.i18n("Wind")},
        {key: "wind_gust_max", title: I18n.i18n("Gusts")},
        {key: "slpres_point", title: I18n.i18n("Pressure")},
        {key: "cldtot_aver", title: I18n.i18n("Clouds")}
    ]
    Column {
        width: root.contentWidth
        Row {
            Label { width: 130; text: I18n.i18n("Time"); font.bold: true; padding: 8 }
            Repeater {
                model: root.columns
                delegate: Label { required property var modelData; width: 114; text: modelData.title; font.bold: true; padding: 8 }
            }
        }
        Repeater {
            model: root.weather.hourlyForecast
            delegate: ItemDelegate {
                id: hour
                required property var modelData
                required property int index
                width: parent.width
                implicitHeight: Math.max(44, implicitContentHeight + topPadding + bottomPadding)
                onClicked: root.readingRequested(modelData.time, mapToItem(null, width / 2, 0))
                background: Rectangle { color: hour.index % 2 ? Kirigami.Theme.alternateBackgroundColor : "transparent" }
                contentItem: Row {
                    Label { width: 122; text: root.weather.formatTime(hour.modelData.time, "ddd HH:mm"); font.bold: true }
                    Repeater {
                        model: root.columns
                        delegate: Column {
                            id: cell
                            required property var modelData
                            readonly property var sample: hour.modelData.cells[modelData.key]
                            width: 114
                            Label { text: cell.sample.value }
                            Label {
                                objectName: "hourlyInterval-" + cell.modelData.key
                                visible: text.length > 0
                                text: cell.sample.intervalLabel
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                            }
                            ToolTip.visible: hover.hovered && sample.sampleText.length > 0
                            ToolTip.text: sample.sampleText
                            HoverHandler { id: hover }
                        }
                    }
                }
            }
        }
    }
}
