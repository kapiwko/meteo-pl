pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Pane {
    id: root
    required property var chart
    objectName: "chartLegendPanel"
    padding: Kirigami.Units.smallSpacing
    background: Rectangle {
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.alternateBackgroundColor
        opacity: 0.5
    }
    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        GridLayout {
            Layout.fillWidth: true
            columns: Math.max(1, Math.min(3, Math.floor(root.availableWidth / 160)))
            uniformCellWidths: true
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: 0
            Repeater {
                model: root.chart.legendFields
                delegate: ItemDelegate {
                    id: series
                    required property var modelData
                    objectName: "chartLegend-" + modelData.key
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    text: modelData.legendLabel
                    checkable: true
                    checked: root.chart.shown(modelData)
                    implicitHeight: Math.max(36, implicitContentHeight + topPadding + bottomPadding)
                    padding: Kirigami.Units.smallSpacing
                    hoverEnabled: !root.chart.exportMode
                    onClicked: root.chart.toggle(modelData.key)
                    ToolTip.text: checked ? I18n.i18n("Hide series: %1").arg(text) : I18n.i18n("Show series: %1").arg(text)
                    ToolTip.visible: hovered && !root.chart.exportMode
                    Accessible.description: ToolTip.text
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing * 2
                        SeriesLegend {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 24
                            seriesStyle: series.modelData
                            darkPalette: root.chart.darkPalette
                            opacity: series.checked ? 1 : 0.25
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: series.text
                            wrapMode: Text.Wrap
                            color: root.chart.textColor; font: Kirigami.Theme.smallFont
                            opacity: series.checked ? 1 : 0.45
                        }
                    }
                }
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing * 3
            Repeater {
                model: root.chart.description.typeLegend
                delegate: RowLayout {
                    id: precipitation
                    required property var modelData
                    width: Math.min(root.availableWidth, implicitWidth)
                    height: Math.max(28, implicitHeight)
                    spacing: Kirigami.Units.smallSpacing
                    // A shared color key still explains the remaining precipitation series.
                    Rectangle {
                        Layout.preferredWidth: 12; Layout.preferredHeight: 12
                        color: root.chart.darkPalette ? precipitation.modelData.darkColor : precipitation.modelData.color
                    }
                    Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: precipitation.modelData.label; color: root.chart.textColor; font: Kirigami.Theme.smallFont; wrapMode: Text.Wrap }
                }
            }
            Repeater {
                model: root.chart.description.eventLegend || []
                delegate: RowLayout {
                    id: event
                    required property var modelData
                    width: Math.min(root.availableWidth, implicitWidth)
                    height: Math.max(28, implicitHeight)
                    spacing: Kirigami.Units.smallSpacing
                    opacity: root.chart.shown(modelData) ? modelData.opacity : 0.3
                    Rectangle {
                        Layout.preferredWidth: 12; Layout.preferredHeight: 12; radius: 6
                        color: event.modelData.hollow ? "transparent"
                            : root.chart.darkPalette ? event.modelData.darkColor : event.modelData.color
                        border.width: 1.5
                        border.color: root.chart.darkPalette ? event.modelData.outline.darkColor : event.modelData.outline.color
                    }
                    Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: event.modelData.label; color: root.chart.textColor; font: Kirigami.Theme.smallFont; wrapMode: Text.Wrap }
                }
            }
            RowLayout {
                visible: root.chart.plotFields.some(f => (f.kind === "stems" || f.kind === "gusts") && f.points.some(p => p.time >= root.chart.startTime && p.time <= root.chart.endTime && root.chart.valid(p.value) && p.value > (root.chart.isRightField(f) ? root.chart.rightBounds.high : root.chart.bounds.high)))
                width: Math.min(root.availableWidth, implicitWidth)
                height: Math.max(28, implicitHeight)
                spacing: Kirigami.Units.smallSpacing
                Rectangle { Layout.preferredWidth: 4; Layout.preferredHeight: 4; radius: 2; color: root.chart.plotStyle("aboveScale") }
                Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.chart.wind ? I18n.i18n("Gust above scale") : I18n.i18n("Maximum precipitation above scale"); color: root.chart.textColor; font: Kirigami.Theme.smallFont; wrapMode: Text.Wrap }
            }
        }
        Repeater {
            model: root.chart.description.colorScales || []
            delegate: ColumnLayout {
                id: scale
                required property var modelData
                objectName: "legendColorScale-" + modelData.key
                Layout.fillWidth: true
                spacing: 3
                opacity: root.chart.shown(modelData) ? 1 : 0.3
                RowLayout {
                    visible: root.chart.description.colorScales.length > 1
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: scale.modelData.label; color: root.chart.textColor; font: Kirigami.Theme.smallFont; wrapMode: Text.Wrap }
                    Label { text: scale.modelData.unit; color: root.chart.textColor; font: Kirigami.Theme.smallFont }
                }
                Item {
                    id: colorBar
                    Layout.fillWidth: true
                    implicitHeight: 10
                    Repeater {
                        model: scale.modelData.bands
                        delegate: Rectangle {
                            required property var modelData
                            x: modelData.from * colorBar.width
                            width: (modelData.to - modelData.from) * colorBar.width
                            height: colorBar.height
                            color: root.chart.darkPalette ? modelData.darkColor : modelData.color
                            opacity: scale.modelData.opacity
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                    implicitHeight: maximum.implicitHeight
                    Label { anchors.left: parent.left; text: scale.modelData.minimumLabel; color: root.chart.textColor; font: Kirigami.Theme.smallFont; opacity: 0.75 }
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: scale.modelData.middleLabel; color: root.chart.textColor; font: Kirigami.Theme.smallFont; opacity: 0.75 }
                    Label {
                        id: maximum
                        anchors.right: parent.right
                        text: scale.modelData.maximumLabel + (root.chart.description.colorScales.length > 1 ? "" : " " + scale.modelData.unit)
                        color: root.chart.textColor; font: Kirigami.Theme.smallFont; opacity: 0.75
                    }
                }
            }
        }
    }
}
