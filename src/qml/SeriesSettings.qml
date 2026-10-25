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
    spacing: 4
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
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                Label { text: "Sposób rysowania" }
                ComboBox {
                    Layout.fillWidth: true
                    model: root.renderers
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.render)
                    onActivated: index => root.change({render: model[index].id})
                }
                Label { text: "Oś wartości" }
                ComboBox {
                    Layout.fillWidth: true
                    model: [{id:"auto",name:"Automatycznie według jednostki"}, {id:"left",name:"Lewa oś"}, {id:"right",name:"Prawa oś"}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.axis)
                    onActivated: index => root.change({axis: model[index].id})
                }
                Label { text: "Kolor · jasny motyw" }
                RowLayout {
                    Rectangle { Layout.preferredWidth: 22; Layout.preferredHeight: 22; color: root.seriesStyle.color; border.color: Kirigami.Theme.disabledTextColor }
                    ToolButton { icon.name: "color-picker"; text: "Wybierz kolor"; display: AbstractButton.IconOnly; onClicked: root.chooseColor("color"); ToolTip.text: text; ToolTip.visible: hovered }
                    TextField {
                        Layout.fillWidth: true
                        text: root.seriesStyle.color
                        maximumLength: 7
                        validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                        onEditingFinished: if (acceptableInput) root.change({color: text})
                        Accessible.name: "Kolor serii w jasnym motywie"
                    }
                }
                Label { text: "Kolor · ciemny motyw" }
                RowLayout {
                    Rectangle { Layout.preferredWidth: 22; Layout.preferredHeight: 22; color: root.seriesStyle.darkColor; border.color: Kirigami.Theme.disabledTextColor }
                    ToolButton { icon.name: "color-picker"; text: "Wybierz kolor"; display: AbstractButton.IconOnly; onClicked: root.chooseColor("darkColor"); ToolTip.text: text; ToolTip.visible: hovered }
                    TextField {
                        Layout.fillWidth: true
                        text: root.seriesStyle.darkColor
                        maximumLength: 7
                        validator: RegularExpressionValidator { regularExpression: /^#[0-9a-fA-F]{6}$/ }
                        onEditingFinished: if (acceptableInput) root.change({darkColor: text})
                        Accessible.name: "Kolor serii w ciemnym motywie"
                    }
                }
                Label { text: "Grubość linii" }
                SpinBox {
                    from: 5; to: 80; stepSize: 5
                    value: Math.round(root.seriesStyle.lineWidth * 10)
                    textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1) + " px"
                    onValueModified: root.change({lineWidth: value / 10})
                }
                Label { text: "Promień punktu"; visible: ["points", "cloudPoint", "storm", "flash"].indexOf(root.seriesStyle.render) >= 0 }
                SpinBox {
                    visible: ["points", "cloudPoint", "storm", "flash"].indexOf(root.seriesStyle.render) >= 0
                    from: 10; to: 100; stepSize: 5
                    value: Math.round(root.seriesStyle.pointSize * 10)
                    textFromValue: (value, locale) => Number(value / 10).toLocaleString(locale, 'f', 1) + " px"
                    onValueModified: root.change({pointSize: value / 10})
                }
                Label { text: "Linia" }
                ComboBox {
                    Layout.fillWidth: true
                    model: [{id:"solid",name:"Ciągła"}, {id:"dashed",name:"Przerywana"}, {id:"dotted",name:"Kropkowana"}]
                    textRole: "name"
                    currentIndex: model.findIndex(item => item.id === root.seriesStyle.lineStyle)
                    onActivated: index => root.change({lineStyle: model[index].id})
                }
                Label { text: "Krycie" }
                SpinBox { from: 0; to: 100; stepSize: 5; value: Math.round(root.seriesStyle.opacity * 100); textFromValue: value => value + "%"; onValueModified: root.change({opacity: value / 100}) }
                Label { text: "Kolejność w warstwie" }
                SpinBox { from: 0; to: 100; value: root.seriesStyle.layer; onValueModified: root.change({layer: value}); ToolTip.text: "Wyższa wartość rysuje serię później w jej warstwie; wypełnienia pozostają pod liniami"; ToolTip.visible: hovered }
                CheckBox {
                    Layout.columnSpan: 2
                    visible: root.seriesStyle.key === "pcpttl_aver" || root.seriesStyle.key === "pcpttl_max"
                    text: "Koloruj według rodzaju opadu"
                    checked: root.seriesStyle.precipitationColors
                    onToggled: root.change({precipitationColors: checked})
                }
                CheckBox {
                    Layout.columnSpan: 2
                    visible: ["storm", "flash"].indexOf(root.seriesStyle.render) >= 0
                    text: "Kolory według liczby wyładowań"
                    checked: root.seriesStyle.eventColors
                    onToggled: root.change({eventColors: checked})
                }
                Label { text: "Wypełnienie"; visible: root.seriesStyle.render === "area" }
                ComboBox {
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
