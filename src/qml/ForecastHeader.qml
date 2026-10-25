pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

GridLayout {
    id: root
    objectName: "forecastHeader"
    required property WeatherModel weather
    signal locationSelected(string name, real latitude, real longitude)
    readonly property bool loading: weatherSymbol.loading
    readonly property var solar: weather.solarTimes
    readonly property bool wide: width >= 660
    columns: wide ? 2 : 1
    columnSpacing: Kirigami.Units.largeSpacing * 3
    rowSpacing: Kirigami.Units.largeSpacing
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing
        PlacePicker {
            Layout.fillWidth: true
            weather: root.weather
            onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude)
        }
        RowLayout {
            visible: root.weather.locationName.length > 0
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            WeatherSymbol {
                id: weatherSymbol
                objectName: "headerWeatherSymbol"
                weather: root.weather
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
            }
            Label {
                text: root.weather.temperatureText
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 2
                font.bold: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Kirigami.Units.smallSpacing / 2
                Label {
                    Layout.fillWidth: true
                    objectName: "forecastSummaryTime"
                    text: root.loading ? "Pobieranie prognozy…" : root.weather.summaryTime + (root.weather.hasData && root.weather.utc ? " UTC" : "")
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: "Wiatr " + root.weather.windText + " · Ciśnienie " + root.weather.pressureText
                    visible: root.weather.hasData
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    ColumnLayout {
        objectName: "solarSummary"
        visible: root.weather.hasData
        Layout.alignment: Qt.AlignBottom | (root.wide ? Qt.AlignRight : Qt.AlignLeft)
        spacing: Kirigami.Units.smallSpacing
        Label {
            text: (root.solar.today ? "Słońce dzisiaj" : "Słońce · " + (root.solar.dateLabel || "")) + (root.weather.utc ? " · UTC" : "")
            visible: root.wide || !root.solar.today || root.weather.utc || !!root.solar.status
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
        }
        RowLayout {
            visible: !root.solar.status
            spacing: Kirigami.Units.largeSpacing * 2
            SolarEvent { title: "Wschód"; time: root.solar.sunriseText || "—"; icon: "icons/sunrise.svg" }
            SolarEvent { title: "Zachód"; time: root.solar.sunsetText || "—"; icon: "icons/sunset.svg" }
        }
        Label { text: root.solar.status || ""; visible: text.length > 0 }
    }
    component SolarEvent: RowLayout {
        id: event
        required property string title
        required property string time
        required property string icon
        spacing: Kirigami.Units.smallSpacing
        Accessible.role: Accessible.StaticText
        Accessible.name: title + " słońca: " + time
        ToolTip.visible: eventHover.hovered
        ToolTip.text: "Orientacyjna godzina · " + (root.weather.utc ? "UTC" : "czas lokalny systemu")
        HoverHandler { id: eventHover }
        Kirigami.Icon {
            source: Qt.resolvedUrl(event.icon)
            isMask: true
            color: Kirigami.Theme.neutralTextColor
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Accessible.ignored: true
        }
        ColumnLayout {
            spacing: 0
            Label { text: event.title; color: Kirigami.Theme.disabledTextColor; font: Kirigami.Theme.smallFont }
            Label { text: event.time; font.bold: true }
        }
    }
}
