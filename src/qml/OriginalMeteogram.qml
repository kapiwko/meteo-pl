pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property real imageScale: 1
    LegacyMeteogramModel { id: original; lookup: root.weather.originalLookupUrl }
    RowLayout {
        Label { text: I18n.i18n("Original ICM meteogram"); font.bold: true; Layout.fillWidth: true }
        ToolButton { text: I18n.i18n("Legend"); checkable: true; id: legendToggle }
        ToolButton { icon.name: "zoom-out"; onClicked: root.imageScale = Math.max(0.5, root.imageScale / 1.25) }
        ToolButton { icon.name: "zoom-in"; onClicked: root.imageScale = Math.min(3, root.imageScale * 1.25) }
        ToolButton { icon.name: "view-refresh"; onClicked: original.reload(); enabled: !original.busy }
    }
    Kirigami.InlineMessage { text: original.error || picture.error; visible: text.length > 0; Layout.fillWidth: true; type: Kirigami.MessageType.Warning }
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        ScrollView {
            id: imageScroll
            anchors.fill: parent
            contentWidth: images.width
            contentHeight: images.height
            clip: true
            Row {
                id: images
                spacing: 8
                BufferedImage {
                    visible: legendToggle.checked
                    width: visible ? implicitWidth * root.imageScale : 0
                    height: implicitHeight * root.imageScale
                    source: visible ? "https://old.meteo.pl/um/metco/leg_um_pl_20120615.png" : ""
                }
                BufferedImage {
                    id: picture
                    source: original.source
                    width: Math.max(imageScroll.availableWidth - (legendToggle.checked ? 280 * root.imageScale : 0), implicitWidth * root.imageScale)
                    height: implicitHeight > 0 ? width * implicitHeight / Math.max(1, implicitWidth) : imageScroll.availableHeight
                }
            }
        }
        BusyIndicator { anchors.centerIn: parent; running: original.busy || picture.loading; visible: running }
        Item {
            anchors.fill: parent
            MapWheelHandler { onZoomRequested: steps => root.imageScale = Math.max(0.5, Math.min(3, root.imageScale * Math.pow(1.25, steps))) }
        }
    }
    Label { text: I18n.i18n("Source: ICM UW / meteo.pl · original image with the author's annotations"); font.pixelSize: 10; opacity: 0.65; wrapMode: Text.WordWrap; Layout.fillWidth: true }
}
