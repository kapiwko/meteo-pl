pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "meteogramDisplaySettings"
    spacing: 12
    property string expandedChart: ""
    property string expandedSeries: ""
    property WeatherModel weather: null
    property string transferMessage: ""
    property bool transferFailed: false
    function transferResult(result, importing) {
        transferFailed = !result.ok;
        transferMessage = result.ok ? (importing ? "Zaimportowano profili: " : "Wyeksportowano profili: ") + result.count : result.error;
        transferNotice.visible = true;
    }
    FileDialog {
        id: importDialog
        objectName: "profileImportDialog"
        title: "Importuj profile meteogramu"
        nameFilters: ["Profile meteogramu (*.json)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: root.transferResult(UserPreferences.importProfiles(selectedFile), true)
    }
    FileDialog {
        id: exportDialog
        objectName: "profileExportDialog"
        property bool allSaved: false
        title: allSaved ? "Eksportuj zapisane profile" : "Eksportuj bieżący profil"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["Profile meteogramu (*.json)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: {
            const result = UserPreferences.exportProfiles(selectedFile, allSaved);
            root.transferResult(result, false);
            if (result.ok) ExportNotifications.saved(selectedFile, "Wyeksportowano profile");
        }
    }
    Label { text: "Układ wykresów i wygląd serii"; font.bold: true }
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
    RowLayout {
        Layout.fillWidth: true
        CheckBox { Layout.fillWidth: true; text: "Własna wysokość wykresów w profilu"; checked: UserPreferences.profileChartHeight > 0; onToggled: UserPreferences.profileChartHeight = checked ? UserPreferences.chartHeight : 0 }
        SpinBox { from: 64; to: 800; stepSize: 10; enabled: UserPreferences.profileChartHeight > 0; value: UserPreferences.profileChartHeight || UserPreferences.chartHeight; textFromValue: value => value + " px"; onValueModified: UserPreferences.profileChartHeight = value }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Button { text: "Importuj…"; icon.name: "document-import"; onClicked: importDialog.open() }
        Button { text: "Eksportuj profil…"; icon.name: "document-export"; onClicked: { exportDialog.allSaved = false; exportDialog.open(); } }
        Button { text: "Eksportuj zapisane…"; icon.name: "document-save-all"; onClicked: { exportDialog.allSaved = true; exportDialog.open(); } ToolTip.text: "Eksportuj wszystkie własne profile i Własny układ"; ToolTip.visible: hovered }
    }
    Kirigami.InlineMessage {
        id: transferNotice
        Layout.fillWidth: true
        visible: false
        text: root.transferMessage
        type: root.transferFailed ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
        showCloseButton: true
    }
    Label {
        Layout.fillWidth: true
        text: "Każdy wykres może łączyć dowolne serie. Próbka przy serii pokazuje jej wygląd. Ustaw kolory, rodzaj rysowania i oś w „Wygląd”. Zmiany zapisują się w wybranym profilu; zmiana gotowego profilu tworzy „Własny układ”."
        wrapMode: Text.WordWrap
        opacity: 0.7
    }
    Repeater {
        model: UserPreferences.chartLayout
        delegate: Frame {
            id: chart
            objectName: "chart-settings-" + modelData.key
            required property var modelData
            required property int index
            Layout.fillWidth: true
            padding: 8
            ColumnLayout {
                anchors.fill: parent
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        Accessible.name: chart.modelData.title
                        checked: chart.modelData.enabled
                        onToggled: UserPreferences.setChartEnabled(chart.modelData.key, checked)
                    }
                    Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: chart.modelData.title; wrapMode: Text.WordWrap }
                    ToolButton { text: "Serie"; checkable: true; checked: root.expandedChart === chart.modelData.key; onClicked: root.expandedChart = checked ? chart.modelData.key : "" }
                    ToolButton { icon.name: "go-up"; enabled: chart.index > 0; onClicked: UserPreferences.moveChart(chart.index, -1); ToolTip.text: "Przenieś wykres wyżej"; ToolTip.visible: hovered }
                    ToolButton { icon.name: "go-down"; enabled: chart.index < UserPreferences.chartLayout.length - 1; onClicked: UserPreferences.moveChart(chart.index, 1); ToolTip.text: "Przenieś wykres niżej"; ToolTip.visible: hovered }
                    ToolButton { icon.name: "edit-delete"; onClicked: UserPreferences.deleteChart(chart.modelData.key); ToolTip.text: "Usuń wykres z tego profilu"; ToolTip.visible: hovered }
                }
                Loader {
                    Layout.fillWidth: true
                    active: root.expandedChart === chart.modelData.key
                    visible: active
                    sourceComponent: Component {
                        ColumnLayout {
                            Layout.fillWidth: true
                            TextField {
                                Layout.fillWidth: true
                                text: chart.modelData.title
                                placeholderText: "Nazwa wykresu"
                                maximumLength: 80
                                onEditingFinished: if (text.trim().length) UserPreferences.setChartOptions(chart.modelData.key, {title: text})
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                CheckBox { Layout.fillWidth: true; text: "Własna wysokość tego wykresu"; checked: (chart.modelData.height || 0) > 0; onToggled: UserPreferences.setChartOptions(chart.modelData.key, {height: checked ? UserPreferences.profileChartHeight || UserPreferences.chartHeight : 0}) }
                                SpinBox { from: 64; to: 800; stepSize: 10; enabled: (chart.modelData.height || 0) > 0; value: chart.modelData.height || UserPreferences.profileChartHeight || UserPreferences.chartHeight; textFromValue: value => value + " px"; onValueModified: UserPreferences.setChartOptions(chart.modelData.key, {height: value}) }
                            }
                            Repeater {
                                model: chart.modelData.series
                                delegate: SeriesSettings {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    chartKey: chart.modelData.key
                                    seriesStyle: modelData
                                    available: !root.weather || root.weather.series[modelData.key] !== undefined
                                    expanded: root.expandedSeries === chartKey + "/" + modelData.key
                                    onToggleAppearance: root.expandedSeries = expanded ? "" : chartKey + "/" + modelData.key
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    id: addSeries
                                    Layout.fillWidth: true
                                    model: UserPreferences.seriesCatalog.filter(series => !chart.modelData.series.some(existing => existing.key === series.key))
                                    textRole: "label"
                                    displayText: currentIndex >= 0 ? currentText : "Wybierz serię…"
                                }
                                Button { text: "Dodaj serię"; enabled: addSeries.currentIndex >= 0; onClicked: UserPreferences.setChartField(chart.modelData.key, addSeries.model[addSeries.currentIndex].key, true) }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 3
                                Label { text: "Zakres osi"; font.bold: true }
                                Label { text: "Minimum" }
                                Label { text: "Maksimum" }
                                Repeater {
                                    model: ["left", "right"]
                                    delegate: RowLayout {
                                        id: axis
                                        required property string modelData
                                        Layout.columnSpan: 3
                                        Layout.fillWidth: true
                                        Label { text: axis.modelData === "left" ? "Lewa" : "Prawa"; Layout.preferredWidth: 65 }
                                        Repeater {
                                            model: ["Min", "Max"]
                                            delegate: TextField {
                                                id: limit
                                                required property string modelData
                                                readonly property string option: axis.modelData + modelData
                                                Layout.fillWidth: true
                                                text: chart.modelData[option] === undefined ? "" : String(chart.modelData[option])
                                                placeholderText: "Automatycznie"
                                                validator: DoubleValidator { locale: "C" }
                                                onEditingFinished: {
                                                    if (!text.length || acceptableInput) {
                                                        const patch = {}; patch[option] = text.length ? Number(text) : null;
                                                        UserPreferences.setChartOptions(chart.modelData.key, patch);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Label { Layout.fillWidth: true; text: "Przy łączeniu różnych jednostek przypisz serie do lewej lub prawej osi. Puste granice oznaczają skalę automatyczną."; wrapMode: Text.WordWrap; font.pixelSize: 11; opacity: 0.65 }
                        }
                    }
                }
            }
        }
    }
    Button { text: "Dodaj wykres"; icon.name: "list-add"; onClicked: root.expandedChart = UserPreferences.createChart("Nowy wykres") }
}
