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
            {id:"line",name:"Linia"}, {id:"points",name:"Punkty"}, {id:"area",name:"Wypełnienie pod linią"},
            {id:"bars",name:"Słupki"}, {id:"stems",name:"Pionowe szpilki"}, {id:"gusts",name:"Poziome kreski"},
            {id:"rightBars",name:"Wąskie słupki"}, {id:"cloudArea",name:"Pokrycie chmur"},
            {id:"cloudPoint",name:"Punkty chmur"}, {id:"direction",name:"Strzałki kierunku"},
            {id:"storm",name:"Wskaźnik burzy"}, {id:"flash",name:"Wyładowania"}, {id:"type",name:"Rodzaj opadu"}
        ];
        if (seriesStyle.lowerKey) result.push({id:"range",name:"Zakres min–max: obszar"}, {id:"rangeStems",name:"Zakres min–max: kreski"});
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
        title: "Kolor serii"
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
        Label { text: root.available ? root.seriesStyle.unit : "brak danych"; opacity: 0.65; font.pixelSize: 11 }
        ToolButton {
            text: "Wygląd"
            icon.name: "configure"
            display: root.width < 520 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
            ToolTip.text: "Wygląd serii"
            ToolTip.visible: hovered
            Accessible.name: ToolTip.text
            checkable: true
            checked: root.expanded
            onClicked: root.toggleAppearance()
        }
        ToolButton {
            icon.name: "list-remove"
            text: "Usuń serię"
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
                    Kirigami.FormData.label: "Rysowanie:"
                    Layout.fillWidth: true
                    model: root.renderers
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.render)
                    onActivated: index => root.change({render: model[index].id})
                }
                ComboBox {
                    Kirigami.FormData.label: "Oś wartości:"
                    Layout.fillWidth: true
                    model: [{id:"auto",name:"Automatycznie według jednostki"}, {id:"left",name:"Lewa oś"}, {id:"right",name:"Prawa oś"}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.axis)
                    onActivated: index => root.change({axis: model[index].id})
                }
                Repeater {
                    model: [{key: "color", label: "Jasny motyw:"}, {key: "darkColor", label: "Ciemny motyw:"}]
                    delegate: RowLayout {
                        id: colorRow
                        required property var modelData
                        Kirigami.FormData.label: modelData.label
                        Layout.fillWidth: true
                        Rectangle { Layout.preferredWidth: 22; Layout.preferredHeight: 22; color: root.seriesStyle[colorRow.modelData.key]; border.color: Kirigami.Theme.disabledTextColor }
                        ToolButton {
                            icon.name: "color-picker"
                            onClicked: root.chooseColor(colorRow.modelData.key)
                            ToolTip.text: "Wybierz kolor"
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
                    Kirigami.FormData.label: "Grubość linii:"
                    from: 5; to: 80; stepSize: 5
                    value: Math.round(root.seriesStyle.lineWidth * 10)
                    textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1) + " px"
                    onValueModified: root.change({lineWidth: value / 10})
                }
                SpinBox {
                    Kirigami.FormData.label: "Promień punktu:"
                    visible: ["points", "cloudPoint", "storm", "flash"].indexOf(root.seriesStyle.render) >= 0
                    from: 10; to: 100; stepSize: 5
                    value: Math.round(root.seriesStyle.pointSize * 10)
                    textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1) + " px"
                    onValueModified: root.change({pointSize: value / 10})
                }
                ComboBox {
                    Kirigami.FormData.label: "Linia:"
                    Layout.fillWidth: true
                    model: [{id:"solid",name:"Ciągła"}, {id:"dashed",name:"Przerywana"}, {id:"dotted",name:"Kropkowana"}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.lineStyle)
                    onActivated: index => root.change({lineStyle: model[index].id})
                }
                SpinBox { Kirigami.FormData.label: "Krycie:"; from: 0; to: 100; stepSize: 5; value: Math.round(root.seriesStyle.opacity * 100); textFromValue: value => value + "%"; onValueModified: root.change({opacity: value / 100}) }
                SpinBox { Kirigami.FormData.label: "Kolejność w warstwie:"; from: 0; to: 100; value: root.seriesStyle.layer; onValueModified: root.change({layer: value}); ToolTip.text: "Wyższa wartość rysuje serię później w jej warstwie; wypełnienia pozostają pod liniami"; ToolTip.visible: hovered }
                CheckBox {
                    visible: root.seriesStyle.key === "pcpttl_aver" || root.seriesStyle.key === "pcpttl_max"
                    text: "Koloruj według rodzaju opadu"
                    checked: root.seriesStyle.precipitationColors
                    onToggled: root.change({precipitationColors: checked})
                }
                CheckBox {
                    visible: ["storm", "flash"].indexOf(root.seriesStyle.render) >= 0
                    text: "Kolory według liczby wyładowań"
                    checked: root.seriesStyle.eventColors
                    onToggled: root.change({eventColors: checked})
                }
                ComboBox {
                    Kirigami.FormData.label: "Wypełnienie:"
                    visible: root.seriesStyle.render === "area"
                    Layout.fillWidth: true
                    model: [{id:"solid",name:"Kolor serii"}, {id:"pressure",name:"Paleta ciśnienia ICM"}, {id:"wind",name:"Paleta wiatru ICM"}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.fill)
                    onActivated: index => root.change({fill: model[index].id})
                }
            }
        }
    }
}
