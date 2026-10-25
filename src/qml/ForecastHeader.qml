pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "forecastHeader"
    required property WeatherModel weather
    property bool expanded: false
    spacing: 4
    GridLayout {
        Layout.fillWidth: true
        columns: root.width < 650 ? 1 : 2
        columnSpacing: 16
        rowSpacing: 4
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Kirigami.Heading { text: root.weather.locationName; level: 2; font.pointSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Kirigami.Icon { source: root.weather.weatherIcon; Layout.preferredWidth: 40; Layout.preferredHeight: 40 }
                Label { text: root.weather.temperatureText; font.pixelSize: 26; font.bold: true }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { Layout.fillWidth: true; text: root.weather.summaryTime; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    Label { Layout.fillWidth: true; text: "Wiatr " + root.weather.windText + " · " + root.weather.pressureText; visible: root.weather.hasData; opacity: 0.65; wrapMode: Text.WordWrap; font.pixelSize: 10 }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: root.width < 650 ? -1 : 320
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.weather.model === "gfs" ? "10 dni · model GFS" : root.weather.model === "um4_120" ? "5 dni · model UM4" : "60 godzin · model UM4"
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                }
                ToolButton { text: "Szczegóły"; icon.name: root.expanded ? "go-up" : "go-down"; checkable: true; checked: root.expanded; onClicked: root.expanded = checked }
            }
            Label { Layout.fillWidth: true; text: "Obliczenia: " + root.weather.runText; visible: root.weather.hasData; opacity: 0.65; font.pixelSize: 10; wrapMode: Text.WordWrap }
        }
    }
    ColumnLayout {
        visible: root.expanded
        Layout.fillWidth: true
        spacing: 4
        Label { Layout.fillWidth: true; text: "Współrzędne: " + Math.abs(root.weather.latitude).toFixed(4) + (root.weather.latitude < 0 ? "°S, " : "°N, ") + Math.abs(root.weather.longitude).toFixed(4) + (root.weather.longitude < 0 ? "°W" : "°E"); wrapMode: Text.WordWrap; font.pixelSize: 11; opacity: 0.7 }
        Label { Layout.fillWidth: true; text: "Słońce: " + root.weather.sunTimes; wrapMode: Text.WordWrap; font.pixelSize: 11; opacity: 0.7 }
        Label { text: root.weather.utc ? "Godziny podane w UTC" : "Godziny podane w czasie lokalnym systemu"; font.pixelSize: 11; opacity: 0.7 }
    }
}
