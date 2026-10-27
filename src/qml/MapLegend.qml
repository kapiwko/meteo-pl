pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Pane {
    id: root
    objectName: "mapLegend"
    required property MapLegendModel legend
    signal closeRequested()
    width: Math.min(320, parent ? Math.max(160, parent.width - 88) : 320)
    height: Math.min(implicitHeight, parent ? Math.max(80, parent.height - 100) : implicitHeight)
    padding: Kirigami.Units.largeSpacing
    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; text: I18n.i18n("Legend"); font.bold: true }
            ToolButton { icon.name: "view-refresh"; enabled: !root.legend.busy; onClicked: root.legend.reload(); Accessible.name: I18n.i18n("Refresh legend") }
            ToolButton { icon.name: "window-close"; onClicked: root.closeRequested(); Accessible.name: I18n.i18n("Hide legend") }
        }
        BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: root.legend.busy; visible: running }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            implicitHeight: content.implicitHeight
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: content
                width: scroll.availableWidth
                spacing: Kirigami.Units.largeSpacing
                Label {
                    Layout.fillWidth: true
                    visible: root.legend.error.length > 0
                    text: root.legend.error
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: !root.legend.busy && !root.legend.error.length && !root.legend.legends.length
                    text: I18n.i18n("No legend is available for this layer.")
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: root.legend.legends
                    delegate: ColumnLayout {
                        id: legendScale
                        required property var modelData
                        objectName: "mapLegendScale-" + modelData.key
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        Label { Layout.fillWidth: true; text: legendScale.modelData.title + (legendScale.modelData.unit.length ? " · " + legendScale.modelData.unit : ""); wrapMode: Text.WordWrap }
                        Row {
                            visible: !legendScale.modelData.categorical
                            Layout.fillWidth: true
                            Layout.preferredHeight: 14
                            Repeater {
                                model: legendScale.modelData.stops
                                delegate: Rectangle {
                                    required property var modelData
                                    width: parent.width / legendScale.modelData.stops.length
                                    height: 14
                                    color: modelData.color
                                }
                            }
                        }
                        Item {
                            id: labels
                            visible: !legendScale.modelData.categorical
                            Layout.fillWidth: true
                            implicitHeight: Kirigami.Theme.smallFont.pixelSize > 0 ? Kirigami.Theme.smallFont.pixelSize + 6 : 22
                            readonly property int count: legendScale.modelData.stops.length <= 8 ? legendScale.modelData.stops.length : 5
                            Repeater {
                                model: labels.count
                                delegate: Label {
                                    required property int index
                                    readonly property int stop: Math.round(index * (legendScale.modelData.stops.length - 1) / Math.max(1, labels.count - 1))
                                    x: Math.max(0, Math.min(labels.width - implicitWidth, (stop + 0.5) / legendScale.modelData.stops.length * labels.width - implicitWidth / 2))
                                    text: legendScale.modelData.stops[stop].label
                                    font: Kirigami.Theme.smallFont
                                }
                            }
                        }
                        Flow {
                            visible: legendScale.modelData.categorical
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing
                            Repeater {
                                model: legendScale.modelData.stops
                                delegate: RowLayout {
                                    required property var modelData
                                    Rectangle { Layout.preferredWidth: 14; Layout.preferredHeight: 14; color: parent.modelData.color; border.color: Kirigami.Theme.disabledTextColor }
                                    Label { text: parent.modelData.label; font: Kirigami.Theme.smallFont }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
