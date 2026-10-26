pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property bool showDays: true
    property var places: UserPreferences.places
    function chooseLocation() { placePicker.chooseLocation(); }
    signal locationSelected(string name, real latitude, real longitude)
    readonly property var days: showDays ? weather.dailyForecast.slice(0, 7) : []
    readonly property var solar: weather.solarTimes
    spacing: Kirigami.Units.largeSpacing

    PlacePicker {
        id: placePicker
        Layout.fillWidth: true
        weather: root.weather
        places: root.places
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
            Label {
                objectName: "compactForecastFacts"
                Layout.fillWidth: true
                visible: root.weather.hasData
                text: I18n.i18n("Wind ") + root.weather.windText + I18n.i18n(" · Pressure ") + root.weather.pressureText
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
            }
        }
        ColumnLayout {
            visible: root.weather.hasData
            spacing: Kirigami.Units.smallSpacing
            SolarEvent {
                objectName: "compactSunrise"
                title: I18n.i18n("Sunrise")
                time: root.solar.sunriseText || "—"
                icon: "icons/sunrise.svg"
                visible: !root.solar.status
            }
            SolarEvent {
                objectName: "compactSunset"
                title: I18n.i18n("Sunset")
                time: root.solar.sunsetText || "—"
                icon: "icons/sunset.svg"
                visible: !root.solar.status
            }
            Label { text: root.solar.status || ""; visible: text.length > 0; wrapMode: Text.WordWrap; Layout.maximumWidth: 100 }
        }
    }
    Kirigami.InlineMessage { objectName: "forecastErrorMessage"; Layout.fillWidth: true; text: root.weather.error; visible: text.length > 0; type: Kirigami.MessageType.Warning }
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
                    placeholder: ""
                    fallback: "weather-none-available"
                    animated: false
                    opacity: status === Kirigami.Icon.Ready ? 1 : 0
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
    component SolarEvent: RowLayout {
        id: event
        required property string title
        required property string time
        required property string icon
        spacing: Kirigami.Units.smallSpacing
        Accessible.role: Accessible.StaticText
        Accessible.name: title + I18n.i18n(": ") + time + (root.weather.utc ? " UTC" : "")
        ToolTip.visible: eventHover.hovered
        ToolTip.text: title + " · " + (root.solar.dateLabel || "") + I18n.i18n(" · approximate time · ") + (root.weather.utc ? "UTC" : I18n.i18n("system local time"))
        HoverHandler { id: eventHover }
        Kirigami.Icon {
            source: Qt.resolvedUrl(event.icon)
            isMask: true
            color: Kirigami.Theme.neutralTextColor
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            Accessible.ignored: true
        }
        Label { text: event.time; font.bold: true; Accessible.ignored: true }
    }
}
