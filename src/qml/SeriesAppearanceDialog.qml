pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "seriesAppearanceDialog"
    property string profileId: UserPreferences.profile
    required property string chartKey
    required property var seriesStyle
    title: I18n.i18n("Series appearance") + " · " + seriesStyle.label
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    width: Math.min(580, parent ? parent.width - 24 : 580)
    height: Math.min(implicitHeight, parent ? parent.height - 24 : 720, 720)
    padding: Kirigami.Units.largeSpacing * 2
    standardButtons: Dialog.Close
    readonly property var renderers: {
        const result = [
            {id:"line",name:I18n.i18n("Line")}, {id:"points",name:I18n.i18n("Points")}, {id:"area",name:I18n.i18n("Fill below line")},
            {id:"bars",name:I18n.i18n("Bars")}, {id:"stems",name:I18n.i18n("Vertical stems")}, {id:"gusts",name:I18n.i18n("Horizontal segments")},
            {id:"rightBars",name:I18n.i18n("Narrow bars")}, {id:"cloudArea",name:I18n.i18n("Cloud cover")},
            {id:"cloudPoint",name:I18n.i18n("Cloud points")}, {id:"direction",name:I18n.i18n("Direction arrows")},
            {id:"storm",name:I18n.i18n("Storm indicator")}, {id:"flash",name:I18n.i18n("Lightning")}, {id:"type",name:I18n.i18n("Precipitation type")}
        ];
        if (seriesStyle.lowerKey) result.push({id:"range",name:I18n.i18n("Min–max range: area")}, {id:"rangeStems",name:I18n.i18n("Min–max range: stems")});
        const allowed = UserPreferences.seriesRenderers(seriesStyle.key);
        return result.filter(renderer => allowed.indexOf(renderer.id) >= 0);
    }
    readonly property bool hasLine: ["line", "area", "cloudArea", "rangeStems", "stems", "gusts", "direction", "storm", "flash"].indexOf(seriesStyle.render) >= 0
    readonly property bool hasLineStyle: ["line", "area", "cloudArea", "rangeStems", "stems", "gusts"].indexOf(seriesStyle.render) >= 0
    readonly property bool automaticColor: ["storm", "flash"].indexOf(seriesStyle.render) >= 0 && seriesStyle.eventColors
    readonly property bool paletteFill: seriesStyle.render === "area" && seriesStyle.fill !== "solid"
    function change(options) {
        if ((options.color !== undefined || options.darkColor !== undefined) && ["storm", "flash"].indexOf(seriesStyle.render) >= 0) options.eventColors = false;
        UserPreferences.setSeriesStyle(chartKey, seriesStyle.key, options, profileId);
    }
    function chooseColor(option) { colorDialog.option = option; colorDialog.selectedColor = seriesStyle[option]; colorDialog.open(); }
    ColorDialog {
        id: colorDialog
        property string option: "color"
        title: I18n.i18n("Series color")
        onAccepted: { const options = {}; options[option] = selectedColor.toString(); root.change(options); }
    }
    contentItem: ScrollView {
        id: formScroll
        implicitWidth: 0
        implicitHeight: styleForm.implicitHeight
        contentWidth: availableWidth
        clip: true
        focus: true
        Keys.onShortcutOverride: event => { if (event.key === Qt.Key_Back) event.accepted = true; }
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Back) {
                event.accepted = true;
                if (Qt.inputMethod.visible || Qt.inputMethod.animating) Qt.inputMethod.hide();
                else root.close();
            }
        }
        SettingsForm {
            id: styleForm
            width: formScroll.availableWidth
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Drawing:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: styleForm.fieldWidth
                model: root.renderers
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === root.seriesStyle.render)
                onActivated: index => root.change({render: model[index].id})
            }
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Value axis:")
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: styleForm.fieldWidth
                model: [{id:"auto",name:I18n.i18n("Automatic by unit")}, {id:"left",name:I18n.i18n("Left axis")}, {id:"right",name:I18n.i18n("Right axis")}]
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === root.seriesStyle.axis)
                onActivated: index => root.change({axis: model[index].id})
            }
            ComboBox {
                Kirigami.FormData.label: I18n.i18n("Fill:")
                visible: root.seriesStyle.render === "area"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: styleForm.fieldWidth
                model: [{id:"solid",name:I18n.i18n("Series color")}, {id:"pressure",name:I18n.i18n("ICM pressure palette")}, {id:"wind",name:I18n.i18n("ICM wind palette")}]
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === root.seriesStyle.fill)
                onActivated: index => root.change({fill: model[index].id})
            }
            Item { Kirigami.FormData.isSection: true; implicitHeight: Kirigami.Units.largeSpacing }
            Repeater {
                model: [{key: "color", label: I18n.i18n("Light theme:")}, {key: "darkColor", label: I18n.i18n("Dark theme:")}]
                delegate: RowLayout {
                    id: colorRow
                    required property var modelData
                    Kirigami.FormData.label: modelData.label
                    enabled: !root.automaticColor
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: styleForm.fieldWidth
                    Rectangle { Layout.preferredWidth: 22; Layout.preferredHeight: 22; color: root.seriesStyle[colorRow.modelData.key]; border.color: Kirigami.Theme.disabledTextColor }
                    ToolButton {
                        icon.name: "color-picker"
                        onClicked: root.chooseColor(colorRow.modelData.key)
                        ToolTip.text: I18n.i18n("Choose color")
                        ToolTip.visible: hovered
                        Accessible.name: ToolTip.text
                    }
                    TextField {
                        objectName: "seriesColor-" + root.chartKey + "-" + root.seriesStyle.key + "-" + colorRow.modelData.key
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: root.seriesStyle[colorRow.modelData.key]
                        maximumLength: 7
                        validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                        onEditingFinished: if (acceptableInput) { const patch = {}; patch[colorRow.modelData.key] = text; root.change(patch); }
                        Accessible.name: colorRow.modelData.label
                    }
                }
            }
            SettingsHint {
                Layout.fillWidth: true
                visible: root.automaticColor || root.paletteFill
                text: root.automaticColor ? I18n.i18n("Colors are set automatically from the weather values.")
                    : I18n.i18n("The palette sets the fill colors. The chosen series color is used for its outline.")
                wrapMode: Text.WordWrap
            }
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
            SpinBox { Kirigami.FormData.label: I18n.i18n("Opacity:"); from: 0; to: 100; stepSize: 5; value: Math.round(root.seriesStyle.opacity * 100); textFromValue: value => value + "%"; onValueModified: root.change({opacity: value / 100}) }
            Item { Kirigami.FormData.isSection: true; implicitHeight: Kirigami.Units.largeSpacing }
            SpinBox {
                Kirigami.FormData.label: I18n.i18n("Line width:")
                visible: root.hasLine
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
                visible: root.hasLineStyle
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: styleForm.fieldWidth
                model: [{id:"solid",name:I18n.i18n("Solid")}, {id:"dashed",name:I18n.i18n("Dashed")}, {id:"dotted",name:I18n.i18n("Dotted")}]
                textRole: "name"
                currentIndex: model.findIndex(item => item.id === root.seriesStyle.lineStyle)
                onActivated: index => root.change({lineStyle: model[index].id})
            }
            CheckBox { id: advanced; text: I18n.i18n("Advanced options") }
            SpinBox { visible: advanced.checked; Kirigami.FormData.label: I18n.i18n("Layer order:"); from: 0; to: 100; value: root.seriesStyle.layer; onValueModified: root.change({layer: value}); ToolTip.text: I18n.i18n("Higher values draw a series later within its layer; fills stay below lines"); ToolTip.visible: hovered }
            SettingsHint {
                visible: advanced.checked
                Layout.fillWidth: true
                text: I18n.i18n("Higher values draw a series later within its layer; fills stay below lines")
                wrapMode: Text.WordWrap
            }
        }
    }
}
