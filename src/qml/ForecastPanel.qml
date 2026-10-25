pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ScrollView {
    id: root
    required property WeatherModel weather
    signal configureRequested()
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        width: root.availableWidth
        spacing: Kirigami.Units.largeSpacing * 2

        RowLayout {
            Layout.fillWidth: true
            Kirigami.Icon { source: root.weather.weatherIcon; Layout.preferredWidth: 56; Layout.preferredHeight: 56 }
            ColumnLayout {
                Layout.fillWidth: true
                Kirigami.Heading { text: root.weather.locationName; level: 2; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: root.weather.summaryTime; opacity: 0.7; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
            Kirigami.Heading { text: root.weather.temperatureText; level: 1 }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Wiatr " + root.weather.windText }
            Item { Layout.fillWidth: true }
            Label { text: root.weather.pressureText }
        }
        Label {
            Layout.fillWidth: true
            text: root.weather.modelName + (root.weather.runText ? " · " + root.weather.runText : "")
            wrapMode: Text.WordWrap
            opacity: 0.7
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.weather.error.length > 0
            type: Kirigami.MessageType.Warning
            text: root.weather.error + (root.weather.hasData ? " Wyświetlam ostatnio zapisaną prognozę." : "")
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.weather.stale
            type: Kirigami.MessageType.Warning
            text: "Zapisana prognoza jest nieaktualna. Ostatnie pobranie: " + root.weather.updatedText
        }
        BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: root.weather.busy; visible: running && !root.weather.hasData }
        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            visible: !root.weather.hasData && !root.weather.busy
            icon.name: "weather-none-available"
            text: "Brak prognozy"
            explanation: "Wybierz lokalizację i pobierz meteogram."
            helpfulAction: Kirigami.Action { text: "Wybierz lokalizację"; onTriggered: root.configureRequested() }
        }
        Repeater {
            model: root.weather.hasData ? [
                { title: "Temperatura", unit: "°C", key: "airtmp_point", secondary: "dwptmp_point", legend: "Punkt rosy", color: "#e67747", bars: false },
                { title: "Opad w przedziale prognozy", unit: "mm", key: "pcpttl_aver", secondary: "", legend: "", color: "#4196dc", bars: true },
                { title: "Wiatr", unit: "m/s", key: "wind10_sd_true_prev_point", secondary: "wind_gust_max", legend: "Porywy", color: "#369b88", bars: false },
                { title: "Ciśnienie na poziomie morza", unit: "hPa", key: "slpres_point", secondary: "", legend: "", color: "#aa7bd4", bars: false },
                { title: "Zachmurzenie", unit: "%", key: "cldtot_aver", secondary: "", legend: "", color: "#8196a8", bars: false }
            ] : []
            delegate: Meteogram {
                required property var modelData
                Layout.fillWidth: true
                visible: points.length > 0
                title: modelData.title
                unit: modelData.unit
                points: root.weather.series[modelData.key] || []
                secondaryPoints: root.weather.series[modelData.secondary] || []
                secondaryTitle: secondaryPoints.length ? modelData.legend : ""
                lineColor: modelData.color
                bars: modelData.bars
                minimum: modelData.key === "cldtot_aver" ? 0 : NaN
                maximum: modelData.key === "cldtot_aver" ? 100 : NaN
                startTime: root.weather.firstTimestamp
                endTime: root.weather.lastTimestamp
            }
        }
        Label {
            Layout.fillWidth: true
            visible: root.weather.hasData
            text: (root.weather.usingCache ? "Z pamięci · " : "Pobrano · ") + root.weather.updatedText + " · czas lokalny systemu"
            opacity: 0.65
            wrapMode: Text.WordWrap
            font.pixelSize: 11
        }
        Label {
            Layout.fillWidth: true
            text: "Prognoza: <a href='https://www.meteo.pl/'>ICM UW / meteo.pl</a> · Modele UM: Met Office"
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }
}
