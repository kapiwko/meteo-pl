pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 12
    property string expandedChart: ""
    function definition(key) { return UserPreferences.chartCatalog.find(chart => chart.key === key); }
    Label { text: "Układ wykresów"; font.bold: true }
    ComboBox {
        Layout.fillWidth: true
        model: UserPreferences.profiles
        textRole: "name"
        currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
        onActivated: index => UserPreferences.profile = model[index].id
    }
    RowLayout {
        Layout.fillWidth: true
        TextField { id: profileName; Layout.fillWidth: true; placeholderText: "Nazwa profilu…"; maximumLength: 80 }
        Button { text: "Zapisz jako nowy"; enabled: profileName.text.trim().length > 0; onClicked: { UserPreferences.createProfile(profileName.text); profileName.clear(); } }
    }
    RowLayout {
        visible: UserPreferences.profile.startsWith("user:")
        Button { text: "Zmień nazwę"; enabled: profileName.text.trim().length > 0; onClicked: { UserPreferences.renameProfile(profileName.text); profileName.clear(); } }
        Button { text: "Usuń profil"; icon.name: "edit-delete"; onClicked: UserPreferences.deleteProfile() }
    }
    Label {
        Layout.fillWidth: true
        text: "Wybierz wykresy, rozwiń ich serie i ustaw kolejność strzałkami. Zmiany zapisanego profilu są pamiętane automatycznie; zmiana gotowego układu tworzy „Własny układ”."
        wrapMode: Text.WordWrap
        opacity: 0.7
    }
    Repeater {
        model: UserPreferences.chartLayout
        delegate: Frame {
            id: chart
            required property var modelData
            required property int index
            readonly property var definition: root.definition(modelData.key)
            Layout.fillWidth: true
            padding: 8
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        Layout.fillWidth: true
                        text: chart.definition ? chart.definition.title : chart.modelData.key
                        checked: chart.modelData.enabled
                        onToggled: UserPreferences.setChartEnabled(chart.modelData.key, checked)
                    }
                    ToolButton { text: "Serie"; checkable: true; checked: root.expandedChart === chart.modelData.key; onClicked: root.expandedChart = checked ? chart.modelData.key : "" }
                    ToolButton { icon.name: "go-up"; enabled: chart.index > 0; onClicked: UserPreferences.moveChart(chart.index, -1); ToolTip.text: "Przenieś wyżej"; ToolTip.visible: hovered }
                    ToolButton { icon.name: "go-down"; enabled: chart.index < UserPreferences.chartLayout.length - 1; onClicked: UserPreferences.moveChart(chart.index, 1); ToolTip.text: "Przenieś niżej"; ToolTip.visible: hovered }
                }
                Flow {
                    Layout.fillWidth: true
                    visible: root.expandedChart === chart.modelData.key
                    Repeater {
                        model: chart.definition ? chart.definition.fields : []
                        delegate: CheckBox {
                            required property var modelData
                            text: modelData.label
                            checked: chart.modelData.fields.indexOf(modelData.key) >= 0
                            onToggled: UserPreferences.setChartField(chart.modelData.key, modelData.key, checked)
                        }
                    }
                }
            }
        }
    }
    Label { text: "Jednostka wiatru"; font.bold: true }
    ComboBox { Layout.fillWidth: true; model: ["m/s", "km/h", "kt"]; currentIndex: model.indexOf(UserPreferences.windUnit); onActivated: UserPreferences.windUnit = currentText }
    CheckBox { text: "Czas UTC"; checked: UserPreferences.utc; onToggled: UserPreferences.utc = checked }
    CheckBox { text: "Cieniuj noc dla wybranej lokalizacji"; checked: UserPreferences.showNight; onToggled: UserPreferences.showNight = checked }
    Label { Layout.fillWidth: true; text: "Legenda ukrywa serie bez zmiany skali. Ctrl + kółko myszy przybliża czas. Kliknięcie wykresu przypina szczegółowy odczyt."; wrapMode: Text.WordWrap; opacity: 0.7 }
}
