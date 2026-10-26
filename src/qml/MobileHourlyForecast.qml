pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ListView {
    id: root
    objectName: "mobileHourlyForecast"
    required property WeatherModel weather
    signal readingRequested(real timestamp, point position)
    model: weather.hourlyForecast
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    function number(value, unit) {
        return value === null || value === undefined ? "—" : Number(value).toLocaleString(Qt.locale(), 'f', unit === "mm" ? 1 : 0) + " " + unit;
    }
    delegate: ItemDelegate {
        id: hour
        required property var modelData
        required property int index
        objectName: "mobileForecastHour-" + index
        width: ListView.view.width
        padding: Kirigami.Units.largeSpacing
        onClicked: root.readingRequested(modelData.time, mapToItem(null, width / 2, height / 2))
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.weather.formatTime(hour.modelData.time, "ddd d MMM · HH:mm")
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
                Label { text: root.number(hour.modelData.airtmp_point, "°C"); font.pixelSize: 22; font.bold: true }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                Repeater {
                    model: [
                        {label: I18n.i18n("Feels like"), field: "wchill_point", unit: "°C"},
                        {label: I18n.i18n("Precipitation"), field: "pcpttl_aver", unit: "mm"},
                        {label: I18n.i18n("Precipitation chance"), field: "pcpttlprob_point", unit: "%"},
                        {label: I18n.i18n("Clouds"), field: "cldtot_aver", unit: "%"},
                        {label: I18n.i18n("Wind"), field: "wind10_sd_true_prev_point", unit: root.weather.windUnit},
                        {label: I18n.i18n("Gusts"), field: "wind_gust_max", unit: root.weather.windUnit},
                        {label: I18n.i18n("Pressure"), field: "slpres_point", unit: "hPa"}
                    ]
                    delegate: Label {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: modelData.label + I18n.i18n(": ") + root.number(hour.modelData[modelData.field], modelData.unit)
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Kirigami.Separator { Layout.fillWidth: true }
        }
    }
}
