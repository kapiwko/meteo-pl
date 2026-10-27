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
    spacing: Kirigami.Units.largeSpacing
    property string expandedChart: ""
    property string expandedSeries: ""
    property WeatherModel weather: null
    property string transferMessage: ""
    property bool transferFailed: false
    PreferenceWriteNotice { visible: UserPreferences.error.length > 0 && !(transferNotice.visible && root.transferFailed) }
    readonly property var currentProfile: UserPreferences.profiles.find(profile => profile.id === UserPreferences.profile) || {name: ""}
    readonly property var availableCharts: UserPreferences.chartCatalog.filter(chart => !UserPreferences.chartLayout.some(existing => existing.key === chart.key))
    function transferResult(result, importing) {
        transferFailed = !result.ok;
        transferMessage = result.ok ? (importing ? I18n.i18n("Profiles imported: ") : I18n.i18n("Profiles exported: ")) + result.count : result.error;
        transferNotice.visible = true;
    }
    FileDialog {
        id: importDialog
        objectName: "profileImportDialog"
        title: I18n.i18n("Import meteogram profiles")
        nameFilters: [I18n.i18n("Meteogram profiles (*.json)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: root.transferResult(UserPreferences.importProfiles(selectedFile), true)
    }
    FileDialog {
        id: exportDialog
        objectName: "profileExportDialog"
        property bool allSaved: false
        title: allSaved ? I18n.i18n("Export saved profiles") : I18n.i18n("Export current profile")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [I18n.i18n("Meteogram profiles (*.json)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: {
            const result = UserPreferences.exportProfiles(selectedFile, allSaved);
            root.transferResult(result, false);
            if (result.ok) ExportNotifications.saved(selectedFile, I18n.i18n("Profiles exported"));
        }
    }
    GroupBox {
        padding: Kirigami.Units.largeSpacing
        title: I18n.i18n("Chart profile")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.largeSpacing
            Kirigami.FormLayout {
                Layout.fillWidth: true
                ComboBox {
                    Kirigami.FormData.label: I18n.i18n("Editing profile:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                    model: UserPreferences.profiles
                    textRole: "name"
                    currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
                    onActivated: index => UserPreferences.profile = model[index].id
                }
                TextField {
                    id: profileName
                    objectName: "profileNameField"
                    Kirigami.FormData.label: I18n.i18n("Name:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 20
                    text: root.currentProfile.name
                    maximumLength: 80
                    onEditingFinished: {
                        if (text.trim().length && text.trim() !== root.currentProfile.name) UserPreferences.renameProfile(text);
                        text = Qt.binding(() => root.currentProfile.name);
                    }
                }
                RowLayout {
                    Kirigami.FormData.label: I18n.i18n("Chart height:")
                    CheckBox {
                        text: I18n.i18n("Custom")
                        checked: UserPreferences.profileChartHeight > 0
                        onToggled: UserPreferences.profileChartHeight = checked ? UserPreferences.chartHeight : 0
                    }
                    SpinBox {
                        from: 64; to: 800; stepSize: 10
                        enabled: UserPreferences.profileChartHeight > 0
                        value: UserPreferences.profileChartHeight || UserPreferences.chartHeight
                        textFromValue: value => value + " px"
                        onValueModified: UserPreferences.profileChartHeight = value
                        Accessible.name: I18n.i18n("Chart height in this profile")
                    }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Button {
                    objectName: "profileDuplicateButton"
                    text: I18n.i18n("Duplicate profile")
                    icon.name: "edit-duplicate"
                    onClicked: {
                        if (UserPreferences.duplicateProfile().length) {
                            profileName.forceActiveFocus();
                            profileName.selectAll();
                        }
                    }
                }
                Button {
                    objectName: "profileDeleteButton"
                    text: I18n.i18n("Delete profile")
                    icon.name: "edit-delete"
                    enabled: UserPreferences.profiles.length > 1
                    onClicked: UserPreferences.deleteProfile()
                    ToolTip.text: enabled ? text : I18n.i18n("At least one profile must remain.")
                    ToolTip.visible: hovered
                }
                Button {
                    objectName: "profileRestoreButton"
                    visible: root.currentProfile.builtin === true
                    enabled: root.currentProfile.modified === true
                    text: I18n.i18n("Restore defaults")
                    icon.name: "edit-undo"
                    onClicked: UserPreferences.resetProfile()
                }
                Button {
                    objectName: "restoreDeletedProfilesButton"
                    visible: UserPreferences.hasDeletedProfiles
                    text: I18n.i18n("Restore deleted presets")
                    icon.name: "edit-undo"
                    onClicked: UserPreferences.restoreDeletedProfiles()
                }
            }
            Kirigami.ActionToolBar {
                Layout.fillWidth: true
                actions: [
                    Kirigami.Action { text: I18n.i18n("Import…"); icon.name: "document-import"; onTriggered: importDialog.open() },
                    Kirigami.Action {
                        text: I18n.i18n("Export"); icon.name: "document-export"
                        children: [
                            Kirigami.Action { text: I18n.i18n("Current profile…"); onTriggered: { exportDialog.allSaved = false; exportDialog.open(); } },
                            Kirigami.Action { text: I18n.i18n("All saved profiles…"); onTriggered: { exportDialog.allSaved = true; exportDialog.open(); } }
                        ]
                    }
                ]
            }
        }
    }
    ProfileUpdateNotice { Layout.fillWidth: true }
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
        text: root.currentProfile.builtin
            ? I18n.i18n("Changes are saved in this profile. Restore defaults to reset this preset.")
            : I18n.i18n("The name and chart changes are saved automatically.")
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.disabledTextColor
    }
    RowLayout {
        Layout.fillWidth: true
        Kirigami.Heading { text: I18n.i18n("Charts in profile"); level: 3; Layout.fillWidth: true }
        ToolButton {
            id: addChartButton
            objectName: "addChartButton"
            text: I18n.i18n("Add chart")
            icon.name: "list-add"
            onClicked: addChartMenu.popup(addChartButton, 0, addChartButton.height)
            Menu {
                id: addChartMenu
                objectName: "addChartMenu"
                MenuItem {
                    text: I18n.i18n("New chart")
                    icon.name: "list-add"
                    onTriggered: root.expandedChart = UserPreferences.createChart(I18n.i18n("New chart"))
                }
                MenuSeparator { visible: root.availableCharts.length > 0 }
                Instantiator {
                    model: root.availableCharts
                    delegate: MenuItem {
                        required property var modelData
                        objectName: "addChart-" + modelData.key
                        text: modelData.title
                        onTriggered: {
                            const key = modelData.key;
                            addChartMenu.close();
                            UserPreferences.addChart(key);
                            root.expandedChart = key;
                        }
                    }
                    onObjectAdded: (index, object) => addChartMenu.insertItem(index + 2, object)
                    onObjectRemoved: (index, object) => addChartMenu.removeItem(object)
                }
            }
        }
    }
    Repeater {
        model: UserPreferences.chartLayout
        delegate: Frame {
            id: chart
            objectName: "chart-settings-" + modelData.key
            required property var modelData
            required property int index
            Layout.fillWidth: true
            padding: Kirigami.Units.smallSpacing
            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox {
                        Accessible.name: chart.modelData.title
                        checked: chart.modelData.enabled
                        onToggled: UserPreferences.setChartEnabled(chart.modelData.key, checked)
                    }
                    Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: chart.modelData.title; wrapMode: Text.WordWrap }
                    ToolButton { text: I18n.i18n("Series"); icon.name: root.expandedChart === chart.modelData.key ? "arrow-up" : "arrow-down"; checkable: true; checked: root.expandedChart === chart.modelData.key; onClicked: root.expandedChart = checked ? chart.modelData.key : "" }
                    ToolButton { icon.name: "go-up"; enabled: chart.index > 0; onClicked: UserPreferences.moveChart(chart.index, -1); ToolTip.text: I18n.i18n("Move chart up"); ToolTip.visible: hovered }
                    ToolButton { icon.name: "go-down"; enabled: chart.index < UserPreferences.chartLayout.length - 1; onClicked: UserPreferences.moveChart(chart.index, 1); ToolTip.text: I18n.i18n("Move chart down"); ToolTip.visible: hovered }
                    ToolButton { icon.name: "edit-delete"; onClicked: UserPreferences.deleteChart(chart.modelData.key); ToolTip.text: I18n.i18n("Remove chart from this profile"); ToolTip.visible: hovered }
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
                                placeholderText: I18n.i18n("Chart name")
                                maximumLength: 80
                                onEditingFinished: if (text.trim().length) UserPreferences.setChartOptions(chart.modelData.key, {title: text})
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                CheckBox { Layout.fillWidth: true; text: I18n.i18n("Custom height for this chart"); checked: (chart.modelData.height || 0) > 0; onToggled: UserPreferences.setChartOptions(chart.modelData.key, {height: checked ? UserPreferences.profileChartHeight || UserPreferences.chartHeight : 0}) }
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
                                    displayText: currentIndex >= 0 ? currentText : I18n.i18n("Choose a series…")
                                }
                                Button { text: I18n.i18n("Add series"); enabled: addSeries.currentIndex >= 0; onClicked: UserPreferences.setChartField(chart.modelData.key, addSeries.model[addSeries.currentIndex].key, true) }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 3
                                Label { text: I18n.i18n("Axis range"); font.bold: true }
                                Label { text: I18n.i18n("Minimum") }
                                Label { text: I18n.i18n("Maximum") }
                                Repeater {
                                    model: ["left", "right"]
                                    delegate: RowLayout {
                                        id: axis
                                        required property string modelData
                                        Layout.columnSpan: 3
                                        Layout.fillWidth: true
                                        Label { text: axis.modelData === "left" ? I18n.i18n("Left") : I18n.i18n("Right"); Layout.preferredWidth: 65 }
                                        Repeater {
                                            model: ["Min", "Max"]
                                            delegate: TextField {
                                                id: limit
                                                required property string modelData
                                                readonly property string option: axis.modelData + modelData
                                                Layout.fillWidth: true
                                                text: chart.modelData[option] === undefined ? "" : String(chart.modelData[option])
                                                placeholderText: I18n.i18n("Automatic")
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
                            Label { Layout.fillWidth: true; text: I18n.i18n("When combining different units, assign series to the left or right axis. Empty bounds use automatic scaling."); wrapMode: Text.WordWrap; font.pixelSize: 11; opacity: 0.65 }
                        }
                    }
                }
            }
        }
    }
}
