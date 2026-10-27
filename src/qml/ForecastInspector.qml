pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Popup {
    id: root
    objectName: "forecastInspector"
    required property WeatherModel weather
    property real timestamp: 0
    property bool pinned: false
    property bool exportBusy: false
    property bool allParameters: false
    property var details: []
    property var highlightedKeys: []
    property string category: ""
    readonly property bool hovered: inspectorHover.hovered
    readonly property alias readingsItem: readingsBackground
    readonly property int columns: readings.columns
    readonly property var categories: [{key: "", name: I18n.i18n("All")},
        {key: "temperature", name: I18n.i18n("Temperature")}, {key: "precipitation", name: I18n.i18n("Precipitation")},
        {key: "wind", name: I18n.i18n("Wind")}, {key: "pressure", name: I18n.i18n("Pressure")},
        {key: "clouds", name: I18n.i18n("Clouds")}, {key: "moisture", name: I18n.i18n("Fog and humidity")},
        {key: "storms", name: I18n.i18n("Storms")}].filter(entry => !entry.key || details.some(detail => categoryFor(detail.group) === entry.key))
    readonly property var visibleDetails: details.filter(detail => !allParameters || !category || categoryFor(detail.group) === category)
    signal pinToggled()
    signal pointerLeft()
    signal exportRequested(string format)
    function categoryFor(group) {
        if (group === "ground") return "temperature";
        if (group === "cloudHeights") return "clouds";
        if (group === "humidity" || group === "visibility") return "moisture";
        return group;
    }
    onAllParametersChanged: category = ""
    parent: Overlay.overlay
    width: Math.min(parent && parent.width >= 1000 ? 690 : 370, parent ? parent.width - 24 : 370)
    padding: 14
    modal: false
    focus: true
    dim: false
    closePolicy: Popup.CloseOnEscape
    onHeightChanged: if (parent && opened) y = Math.max(8, Math.min(y, parent.height - height - 8))
    HoverHandler { id: inspectorHover; onHoveredChanged: if (!hovered) root.pointerLeft() }
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Label {
                text: root.weather.formatTime(root.timestamp, "ddd d MMM · HH:mm t")
                font.bold: true
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            ToolButton {
                icon.name: root.pinned ? "pin" : "window-pin"
                checkable: true
                checked: root.pinned
                onClicked: root.pinToggled()
                ToolTip.text: root.pinned ? I18n.i18n("Unpin reading") : I18n.i18n("Pin reading")
                ToolTip.visible: hovered
                Accessible.name: ToolTip.text
            }
            ToolButton {
                icon.name: "document-export"
                enabled: !root.exportBusy && root.visibleDetails.length > 0
                onClicked: { if (!root.pinned) root.pinToggled(); readingExportMenu.popup(); }
                ToolTip.text: I18n.i18n("Export visible parameters")
                ToolTip.visible: hovered
                Accessible.name: ToolTip.text
                Menu {
                    id: readingExportMenu
                    MenuItem { text: I18n.i18n("Save reading as PNG…"); icon.name: "image-png"; onTriggered: root.exportRequested("png") }
                    MenuItem { text: I18n.i18n("Save reading as CSV…"); icon.name: "text-csv"; onTriggered: root.exportRequested("csv") }
                }
            }
            ToolButton { icon.name: "dialog-close"; onClicked: root.close(); Accessible.name: I18n.i18n("Close reading") }
        }
        Label {
            text: root.pinned ? I18n.i18n("Reading pinned · click a chart to unpin") : I18n.i18n("Click a chart to pin the reading")
            opacity: 0.6
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        TabBar {
            objectName: "inspectorCategories"
            Layout.fillWidth: true
            implicitWidth: 0
            clip: true
            visible: root.allParameters
            currentIndex: Math.max(0, root.categories.findIndex(entry => entry.key === root.category))
            TabButton { baselineOffset: height / 2; text: I18n.i18n("All"); width: implicitWidth; onClicked: root.category = "" }
            Repeater {
                model: root.categories.slice(1)
                delegate: TabButton {
                    baselineOffset: height / 2
                    required property var modelData
                    width: implicitWidth
                    text: modelData.name
                    onClicked: root.category = modelData.key
                }
            }
        }
        ScrollView {
            id: inspectorScroll
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(40, Math.min(readings.implicitHeight, root.parent ? root.parent.height - (root.allParameters ? 220 : 160) : 440, 530))
            contentWidth: availableWidth
            clip: true
            Rectangle {
                id: readingsBackground
                objectName: "inspectorReadings"
                readonly property string exportRevision: JSON.stringify({
                    time: root.timestamp, details: root.visibleDetails, highlighted: root.highlightedKeys,
                    columns: readings.columns, background: String(color)
                })
                width: inspectorScroll.availableWidth
                implicitHeight: readings.implicitHeight
                color: Kirigami.Theme.backgroundColor
                GridLayout {
                    id: readings
                    width: parent.width
                    columns: width >= 560 ? 2 : 1
                    rowSpacing: 2
                    columnSpacing: 12
                    Repeater {
                        model: root.visibleDetails
                        delegate: Rectangle {
                            id: reading
                            required property var modelData
                            Layout.preferredWidth: Math.max(1, (readings.width - readings.columnSpacing * (readings.columns - 1)) / readings.columns)
                            Layout.minimumWidth: 0
                            Layout.fillHeight: true
                            implicitHeight: Math.max(nameColumn.implicitHeight, valueLabel.implicitHeight) + 12
                            color: root.highlightedKeys.indexOf(modelData.key) >= 0 ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                Rectangle {
                                    Layout.preferredWidth: 4
                                    Layout.preferredHeight: 15
                                    color: Kirigami.Theme.backgroundColor.hslLightness < 0.5 ? reading.modelData.darkColor : reading.modelData.color
                                }
                                ColumnLayout {
                                    id: nameColumn
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 1
                                    Label { text: reading.modelData.label; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                                    Label {
                                        visible: text.length > 0
                                        text: reading.modelData.sampleText || ""
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                    }
                                }
                                Label { id: valueLabel; text: reading.modelData.value; font.bold: true; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }
        }
    }
}
