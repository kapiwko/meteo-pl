pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "series-settings-" + chartKey + "/" + seriesStyle.key
    required property string chartKey
    required property var seriesStyle
    property bool expanded: false
    property bool available: true
    signal toggleAppearance()
    readonly property var renderers: {
        const result = [
            {id:"line",name:I18n.i18n("Line")}, {id:"points",name:I18n.i18n("Points")}, {id:"area",name:I18n.i18n("Fill below line")},
            {id:"bars",name:I18n.i18n("Bars")}, {id:"stems",name:I18n.i18n("Vertical stems")}, {id:"gusts",name:I18n.i18n("Horizontal segments")},
            {id:"rightBars",name:I18n.i18n("Narrow bars")}, {id:"cloudArea",name:I18n.i18n("Cloud cover")},
            {id:"cloudPoint",name:I18n.i18n("Cloud points")}, {id:"direction",name:I18n.i18n("Direction arrows")},
            {id:"storm",name:I18n.i18n("Storm indicator")}, {id:"flash",name:I18n.i18n("Lightning")}, {id:"type",name:I18n.i18n("Precipitation type")}
        ];
        if (seriesStyle.lowerKey) result.push({id:"range",name:I18n.i18n("Min–max range: area")}, {id:"rangeStems",name:I18n.i18n("Min–max range: stems")});
        return result;
    }
    function change(options) {
        if ((options.color !== undefined || options.darkColor !== undefined) && ["storm", "flash"].indexOf(seriesStyle.render) >= 0) options.eventColors = false;
        UserPreferences.setSeriesStyle(chartKey, seriesStyle.key, options);
    }
    function chooseColor(option) { colorDialog.option = option; colorDialog.selectedColor = seriesStyle[option]; colorDialog.open(); }
    ColorDialog {
        id: colorDialog
        property string option: "color"
        title: I18n.i18n("Series color")
        onAccepted: { const options = {}; options[option] = selectedColor.toString(); root.change(options); }
    }
    spacing: Kirigami.Units.smallSpacing
    RowLayout {
        Layout.fillWidth: true
        CheckBox {
            Accessible.name: root.seriesStyle.label
            checked: root.seriesStyle.enabled
            onToggled: UserPreferences.setChartField(root.chartKey, root.seriesStyle.key, checked)
        }
        Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: root.seriesStyle.label; wrapMode: Text.WordWrap }
        SeriesLegend { seriesStyle: root.seriesStyle; Layout.preferredWidth: 42; opacity: root.seriesStyle.enabled ? 1 : 0.3 }
        Label { text: root.available ? root.seriesStyle.unit : I18n.i18n("no data"); opacity: 0.65; font.pixelSize: 11 }
        ToolButton {
            text: I18n.i18n("Appearance")
            icon.name: "configure"
            display: root.width < 520 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
            ToolTip.text: I18n.i18n("Series appearance")
            ToolTip.visible: hovered
            Accessible.name: ToolTip.text
            checkable: true
            checked: root.expanded
            onClicked: root.toggleAppearance()
        }
        ToolButton {
            icon.name: "list-remove"
            text: I18n.i18n("Remove series")
            display: AbstractButton.IconOnly
            ToolTip.text: text
            ToolTip.visible: hovered
            onClicked: UserPreferences.removeSeries(root.chartKey, root.seriesStyle.key)
        }
    }
    Loader {
        Layout.fillWidth: true
        active: root.expanded
        visible: active
        sourceComponent: Component {
            Kirigami.FormLayout {
                Layout.fillWidth: true
                ComboBox {
                    Kirigami.FormData.label: I18n.i18n("Drawing:")
                    Layout.fillWidth: true
                    model: root.renderers
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.render)
                    onActivated: index => root.change({render: model[index].id})
                }
                ComboBox {
                    Kirigami.FormData.label: I18n.i18n("Value axis:")
                    Layout.fillWidth: true
                    model: [{id:"auto",name:I18n.i18n("Automatic by unit")}, {id:"left",name:I18n.i18n("Left axis")}, {id:"right",name:I18n.i18n("Right axis")}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.axis)
                    onActivated: index => root.change({axis: model[index].id})
                }
                Repeater {
                    model: [{key: "color", label: I18n.i18n("Light theme:")}, {key: "darkColor", label: I18n.i18n("Dark theme:")}]
                    delegate: RowLayout {
                        id: colorRow
                        required property var modelData
                        Kirigami.FormData.label: modelData.label
                        Layout.fillWidth: true
                        Rectangle { Layout.preferredWidth: 22; Layout.preferredHeight: 22; color: root.seriesStyle[colorRow.modelData.key]; border.color: Kirigami.Theme.disabledTextColor }
                        ToolButton {
                            icon.name: "color-picker"
                            onClicked: root.chooseColor(colorRow.modelData.key)
                            ToolTip.text: I18n.i18n("Choose color")
                            ToolTip.visible: hovered
                            Accessible.name: ToolTip.text
                        }
                        TextField {
                            Layout.fillWidth: true
                            text: root.seriesStyle[colorRow.modelData.key]
                            maximumLength: 7
                            validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                            onEditingFinished: if (acceptableInput) { const patch = {}; patch[colorRow.modelData.key] = text; root.change(patch); }
                            Accessible.name: colorRow.modelData.label
                        }
                    }
                }
                SpinBox {
                    Kirigami.FormData.label: I18n.i18n("Line width:")
                    from: 5; to: 80; stepSize: 5
                    value: Math.round(root.seriesStyle.lineWidth * 10)
                    textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1) + " px"
                    onValueModified: root.change({lineWidth: value / 10})
                }
                SpinBox {
                    Kirigami.FormData.label: I18n.i18n("Point radius:")
                    visible: ["points", "cloudPoint", "storm", "flash"].indexOf(root.seriesStyle.render) >= 0
                    from: 10; to: 100; stepSize: 5
                    value: Math.round(root.seriesStyle.pointSize * 10)
                    textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1) + " px"
                    onValueModified: root.change({pointSize: value / 10})
                }
                ComboBox {
                    Kirigami.FormData.label: I18n.i18n("Line:")
                    Layout.fillWidth: true
                    model: [{id:"solid",name:I18n.i18n("Solid")}, {id:"dashed",name:I18n.i18n("Dashed")}, {id:"dotted",name:I18n.i18n("Dotted")}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.lineStyle)
                    onActivated: index => root.change({lineStyle: model[index].id})
                }
                SpinBox { Kirigami.FormData.label: I18n.i18n("Opacity:"); from: 0; to: 100; stepSize: 5; value: Math.round(root.seriesStyle.opacity * 100); textFromValue: value => value + "%"; onValueModified: root.change({opacity: value / 100}) }
                SpinBox { Kirigami.FormData.label: I18n.i18n("Layer order:"); from: 0; to: 100; value: root.seriesStyle.layer; onValueModified: root.change({layer: value}); ToolTip.text: I18n.i18n("Higher values draw a series later within its layer; fills stay below lines"); ToolTip.visible: hovered }
                CheckBox {
                    visible: root.seriesStyle.key === "pcpttl_aver" || root.seriesStyle.key === "pcpttl_max"
                    text: I18n.i18n("Color by precipitation type")
                    checked: root.seriesStyle.precipitationColors
                    onToggled: root.change({precipitationColors: checked})
                }
                CheckBox {
                    visible: ["storm", "flash"].indexOf(root.seriesStyle.render) >= 0
                    text: I18n.i18n("Color by lightning count")
                    checked: root.seriesStyle.eventColors
                    onToggled: root.change({eventColors: checked})
                }
                ComboBox {
                    Kirigami.FormData.label: I18n.i18n("Fill:")
                    visible: root.seriesStyle.render === "area"
                    Layout.fillWidth: true
                    model: [{id:"solid",name:I18n.i18n("Series color")}, {id:"pressure",name:I18n.i18n("ICM pressure palette")}, {id:"wind",name:I18n.i18n("ICM wind palette")}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.fill)
                    onActivated: index => root.change({fill: model[index].id})
                }
            }
        }
    }
}
