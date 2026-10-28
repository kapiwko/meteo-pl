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
    property string profileId: UserPreferences.profile
    property var layout: UserPreferences.chartLayoutForProfile(profileId)
    property int profileHeight: UserPreferences.chartHeightForProfile(profileId)
    onProfileIdChanged: { refreshProfile(); expandedChart = ""; expandedSeries = ""; }
    function refreshProfile() {
        layout = UserPreferences.chartLayoutForProfile(profileId);
        profileHeight = UserPreferences.chartHeightForProfile(profileId);
    }
    Connections {
        target: UserPreferences
        function onDisplayChanged() {
            if (!UserPreferences.profiles.some(profile => profile.id === root.profileId)) root.profileId = UserPreferences.profile;
            root.refreshProfile();
        }
    }
    property string expandedChart: ""
    property string expandedSeries: ""
    property WeatherModel weather: null
    readonly property var editingChart: root.layout.find(chart => chart.series.some(series => chart.key + "/" + series.key === expandedSeries)) || null
    readonly property var editingSeries: editingChart ? editingChart.series.find(series => editingChart.key + "/" + series.key === expandedSeries) || null : null
    onVisibleChanged: if (!visible) expandedSeries = ""
    onEditingSeriesChanged: if (!editingSeries && expandedSeries.length) expandedSeries = ""
    Loader {
        active: root.visible && root.editingSeries !== null
        visible: false
        onLoaded: {
            const dialog = item as SeriesAppearanceDialog;
            if (dialog) dialog.open();
        }
        sourceComponent: SeriesAppearanceDialog {
            profileId: root.profileId
            chartKey: root.editingChart ? root.editingChart.key : ""
            seriesStyle: root.editingSeries || ({})
            onClosed: root.expandedSeries = ""
        }
    }
    property string transferMessage: ""
    property bool transferFailed: false
    PreferenceWriteNotice { visible: UserPreferences.error.length > 0 && !(transferNotice.visible && root.transferFailed) }
    readonly property var currentProfile: UserPreferences.profiles.find(profile => profile.id === root.profileId) || {name: ""}
    readonly property var availableCharts: UserPreferences.chartCatalog.filter(chart => !root.layout.some(existing => existing.key === chart.key))
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
        onAccepted: {
            const result = UserPreferences.importProfiles(selectedFile, false);
            root.transferResult(result, true);
            if (result.ok && result.ids.length) root.profileId = result.ids[0];
        }
    }
    FileDialog {
        id: exportDialog
        objectName: "profileExportDialog"
        property var profileIds: []
        title: I18n.i18n("Export profiles")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [I18n.i18n("Meteogram profiles (*.json)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: {
            const result = UserPreferences.exportProfilesByIds(selectedFile, profileIds);
            root.transferResult(result, false);
            if (result.ok) ExportNotifications.saved(selectedFile, I18n.i18n("Profiles exported"));
        }
    }
    readonly property Dialog exportSelectionDialog: Dialog {
        id: exportSelection
        objectName: "profileExportSelection"
        property var selectedIds: []
        title: I18n.i18n("Select profiles to export")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        width: Math.min(480, parent ? parent.width - 24 : 480)
        height: Math.min(implicitHeight, parent ? parent.height - 24 : 600)
        onOpened: selectedIds = [root.profileId]
        contentItem: ScrollView {
            id: selectionScroll
            focus: true
            Keys.onShortcutOverride: event => { if (event.key === Qt.Key_Back) event.accepted = true; }
            Keys.onPressed: event => { if (event.key === Qt.Key_Back) { event.accepted = true; exportSelection.reject(); } }
            implicitHeight: selectionList.implicitHeight
            implicitWidth: 0
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: selectionList
                width: selectionScroll.availableWidth
                RowLayout {
                    Button { text: I18n.i18n("Select all"); onClicked: exportSelection.selectedIds = UserPreferences.profiles.map(profile => profile.id) }
                    Button { text: I18n.i18n("Clear selection"); onClicked: exportSelection.selectedIds = [] }
                }
                Repeater {
                    model: UserPreferences.profiles
                    delegate: CheckBox {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name
                        checked: exportSelection.selectedIds.indexOf(modelData.id) >= 0
                        onToggled: {
                            const ids = exportSelection.selectedIds.filter(id => id !== modelData.id);
                            if (checked) ids.push(modelData.id);
                            exportSelection.selectedIds = ids;
                        }
                    }
                }
            }
        }
        footer: DialogButtonBox {
            Button { text: I18n.i18n("Export…"); enabled: exportSelection.selectedIds.length > 0; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: I18n.i18n("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: { exportDialog.profileIds = selectedIds; Qt.callLater(() => exportDialog.open()); }
    }
    SettingsSection {
        title: I18n.i18n("Chart profile")
        Layout.fillWidth: true
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Kirigami.FormLayout {
                id: profileForm
                Layout.fillWidth: true
                RowLayout {
                    Kirigami.FormData.label: I18n.i18n("Editing profile:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                    ComboBox {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: UserPreferences.profiles
                        textRole: "name"
                        currentIndex: model.findIndex(profile => profile.id === root.profileId)
                        onActivated: index => root.profileId = model[index].id
                    }
                    ToolButton {
                        id: profileActionsButton
                        objectName: "profileActionsButton"
                        icon.name: "overflow-menu"
                        ToolTip.text: I18n.i18n("More options")
                        ToolTip.visible: hovered
                        Accessible.name: ToolTip.text
                        onClicked: profileActionsMenu.popup(profileActionsButton, 0, height)
                        Menu {
                            id: profileActionsMenu
                            objectName: "profileActionsMenu"
                            focus: true
                            onOpened: contentItem.forceActiveFocus()
                            Connections {
                                target: profileActionsMenu.contentItem.Keys
                                function onShortcutOverride(event) {
                                    if (event.key === Qt.Key_Back) event.accepted = true;
                                }
                                function onPressed(event) {
                                    if (event.key === Qt.Key_Back) {
                                        event.accepted = true;
                                        profileActionsMenu.close();
                                    }
                                }
                            }
                            MenuItem {
                                objectName: "profileDuplicateButton"
                                text: I18n.i18n("Duplicate profile")
                                icon.name: "edit-duplicate"
                                onTriggered: {
                                    const id = UserPreferences.duplicateProfile(root.profileId);
                                    if (id.length) { root.profileId = id; Qt.callLater(() => { profileName.forceActiveFocus(); profileName.selectAll(); }); }
                                }
                            }
                            MenuItem {
                                objectName: "profileRestoreButton"
                                visible: root.currentProfile.builtin === true
                                enabled: root.currentProfile.modified === true
                                text: I18n.i18n("Restore defaults")
                                icon.name: "edit-undo"
                                onTriggered: UserPreferences.resetProfile(root.profileId)
                            }
                            MenuItem {
                                objectName: "restoreDeletedProfilesButton"
                                visible: UserPreferences.hasDeletedProfiles
                                text: I18n.i18n("Restore deleted presets")
                                icon.name: "edit-undo"
                                onTriggered: UserPreferences.restoreDeletedProfiles()
                            }
                            MenuSeparator {}
                            MenuItem {
                                objectName: "profileDeleteButton"
                                text: I18n.i18n("Delete profile")
                                icon.name: "edit-delete"
                                enabled: UserPreferences.profiles.length > 1
                                onTriggered: UserPreferences.deleteProfile(root.profileId)
                            }
                        }
                    }
                }
                Button {
                    objectName: "useProfileButton"
                    text: root.profileId === UserPreferences.profile ? I18n.i18n("Used in the forecast") : I18n.i18n("Use for forecast")
                    icon.name: "dialog-ok-apply"
                    enabled: root.profileId !== UserPreferences.profile
                    onClicked: UserPreferences.profile = root.profileId
                }
                TextField {
                    id: profileName
                    objectName: "profileNameField"
                    Kirigami.FormData.label: I18n.i18n("Name:")
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                    text: root.currentProfile.name
                    maximumLength: 80
                    onEditingFinished: {
                        if (text.trim().length && text.trim() !== root.currentProfile.name) UserPreferences.renameProfile(text, root.profileId);
                        text = Qt.binding(() => root.currentProfile.name);
                    }
                }
                RowLayout {
                    Kirigami.FormData.label: I18n.i18n("Chart height:")
                    CheckBox {
                        text: I18n.i18n("Custom")
                        checked: root.profileHeight > 0
                        onToggled: UserPreferences.setProfileChartHeightForProfile(root.profileId, checked ? UserPreferences.chartHeight : 0)
                    }
                    SpinBox {
                        from: 64; to: 800; stepSize: 10
                        enabled: root.profileHeight > 0
                        value: root.profileHeight || UserPreferences.chartHeight
                        textFromValue: value => value + " px"
                        onValueModified: UserPreferences.setProfileChartHeightForProfile(root.profileId, value)
                        Accessible.name: I18n.i18n("Chart height in this profile")
                    }
                }
                Kirigami.ActionToolBar {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                    actions: [
                        Kirigami.Action { text: I18n.i18n("Import…"); icon.name: "document-import"; onTriggered: importDialog.open() },
                        Kirigami.Action { text: I18n.i18n("Export…"); icon.name: "document-export"; onTriggered: exportSelection.open() }
                    ]
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.currentProfile.builtin
                    ? I18n.i18n("Changes are saved in this profile. Restore defaults to reset this preset.")
                    : I18n.i18n("The name and chart changes are saved automatically.")
                wrapMode: Text.WordWrap
                opacity: 0.7
            }
        }
    }
    ProfileUpdateNotice { Layout.fillWidth: true; profileId: root.profileId }
    Kirigami.InlineMessage {
        id: transferNotice
        Layout.fillWidth: true
        visible: false
        text: root.transferMessage
        type: root.transferFailed ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
        showCloseButton: true
    }
    Pane {
        Layout.fillWidth: true
        padding: 0
        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        contentItem: ColumnLayout {
            spacing: 0
            Kirigami.InlineViewHeader {
                Layout.fillWidth: true
                text: I18n.i18n("Charts in profile")
                actions: [Kirigami.Action {
                    text: I18n.i18n("Add chart")
                    icon.name: "list-add"
                    displayComponent: ToolButton {
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
                                onTriggered: root.expandedChart = UserPreferences.createChart(I18n.i18n("New chart"), root.profileId)
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
                                        UserPreferences.addChart(key, root.profileId);
                                        root.expandedChart = key;
                                    }
                                }
                                onObjectAdded: (index, object) => addChartMenu.insertItem(index + 2, object)
                                onObjectRemoved: (index, object) => addChartMenu.removeItem(object)
                            }
                        }
                    }
                }]
            }
            Repeater {
                model: root.layout
                delegate: Pane {
                    id: chartEntry
                    objectName: "chart-settings-" + modelData.key
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    padding: 0
                    background: null
                    contentItem: ColumnLayout {
                        spacing: 0
                        Kirigami.Separator {
                            visible: chartEntry.index > 0
                            Layout.fillWidth: true
                        }
                        ItemDelegate {
                            Layout.fillWidth: true
                            Accessible.name: chartEntry.modelData.title
                            onClicked: root.expandedChart = root.expandedChart === chartEntry.modelData.key ? "" : chartEntry.modelData.key
                            contentItem: RowLayout {
                                CheckBox {
                                    Accessible.name: chartEntry.modelData.title
                                    checked: chartEntry.modelData.enabled
                                    onToggled: UserPreferences.setChartEnabled(chartEntry.modelData.key, checked, root.profileId)
                                }
                                Label { Layout.fillWidth: true; Layout.minimumWidth: 0; text: chartEntry.modelData.title; wrapMode: Text.WordWrap }
                                Kirigami.ActionToolBar {
                                    alignment: Qt.AlignRight
                                    Layout.preferredWidth: maximumContentWidth
                                    Layout.maximumWidth: Math.max(0, chartEntry.availableWidth * 0.65)
                                    actions: [
                                        Kirigami.Action {
                                            text: I18n.i18n("Series")
                                            icon.name: root.expandedChart === chartEntry.modelData.key ? "arrow-up" : "arrow-down"
                                            checkable: true
                                            displayHint: Kirigami.DisplayHint.KeepVisible
                                            checked: root.expandedChart === chartEntry.modelData.key
                                            onTriggered: root.expandedChart = checked ? chartEntry.modelData.key : ""
                                        },
                                        Kirigami.Action { text: I18n.i18n("Move chart up"); icon.name: "go-up"; enabled: chartEntry.index > 0; displayHint: Kirigami.DisplayHint.IconOnly; onTriggered: UserPreferences.moveChart(chartEntry.index, -1, root.profileId) },
                                        Kirigami.Action { text: I18n.i18n("Move chart down"); icon.name: "go-down"; enabled: chartEntry.index < root.layout.length - 1; displayHint: Kirigami.DisplayHint.IconOnly; onTriggered: UserPreferences.moveChart(chartEntry.index, 1, root.profileId) },
                                        Kirigami.Action { text: I18n.i18n("Remove chart from this profile"); icon.name: "edit-delete"; displayHint: Kirigami.DisplayHint.IconOnly; onTriggered: UserPreferences.deleteChart(chartEntry.modelData.key, root.profileId) }
                                    ]
                                }
                            }
                        }
                        Loader {
                            Layout.fillWidth: true
                            Layout.margins: Kirigami.Units.largeSpacing
                            active: root.expandedChart === chartEntry.modelData.key
                            visible: active
                            sourceComponent: Component {
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.largeSpacing
                                    Kirigami.FormLayout {
                                        id: chartForm
                                        Layout.fillWidth: true
                                        TextField {
                                            Kirigami.FormData.label: I18n.i18n("Chart name")
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                                            text: chartEntry.modelData.title
                                            maximumLength: 80
                                            onEditingFinished: if (text.trim().length) UserPreferences.setChartOptions(chartEntry.modelData.key, {title: text}, root.profileId)
                                        }
                                        RowLayout {
                                            Kirigami.FormData.label: I18n.i18n("Chart height:")
                                            CheckBox { text: I18n.i18n("Custom"); Accessible.name: I18n.i18n("Custom height for this chart"); checked: (chartEntry.modelData.height || 0) > 0; onToggled: UserPreferences.setChartOptions(chartEntry.modelData.key, {height: checked ? root.profileHeight || UserPreferences.chartHeight : 0}, root.profileId) }
                                            SpinBox { from: 64; to: 800; stepSize: 10; enabled: (chartEntry.modelData.height || 0) > 0; value: chartEntry.modelData.height || root.profileHeight || UserPreferences.chartHeight; textFromValue: value => value + " px"; onValueModified: UserPreferences.setChartOptions(chartEntry.modelData.key, {height: value}, root.profileId) }
                                        }
                                    }
                                    Repeater {
                                        model: chartEntry.modelData.series
                                        delegate: SeriesSettings {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            profileId: root.profileId
                                            chartKey: chartEntry.modelData.key
                                            seriesStyle: modelData
                                            available: !root.weather || root.weather.series[modelData.key] !== undefined
                                            expanded: root.expandedSeries === chartKey + "/" + modelData.key
                                            onRemoveRequested: UserPreferences.removeSeries(chartEntry.modelData.key, modelData.key, root.profileId)
                                            onToggleAppearance: root.expandedSeries = expanded ? "" : chartKey + "/" + modelData.key
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.smallSpacing
                                        ComboBox {
                                            id: addSeries
                                            Layout.fillWidth: true
                                            model: UserPreferences.seriesCatalog.filter(series => !chartEntry.modelData.series.some(existing => existing.key === series.key))
                                            textRole: "label"
                                            displayText: currentIndex >= 0 ? currentText : I18n.i18n("Choose a series…")
                                        }
                                        Button { text: I18n.i18n("Add series"); enabled: addSeries.currentIndex >= 0; onClicked: UserPreferences.setChartField(chartEntry.modelData.key, addSeries.model[addSeries.currentIndex].key, true, root.profileId) }
                                    }
                                    Kirigami.FormLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: Kirigami.Units.largeSpacing
                                        twinFormLayouts: [chartForm]
                                        Repeater {
                                            model: ["left", "right"]
                                            delegate: RowLayout {
                                                id: axis
                                                required property string modelData
                                                Kirigami.FormData.label: modelData === "left" ? I18n.i18n("Left axis") : I18n.i18n("Right axis")
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                Repeater {
                                                    model: ["Min", "Max"]
                                                    delegate: ColumnLayout {
                                                        id: bound
                                                        required property string modelData
                                                        readonly property string option: axis.modelData + modelData
                                                        Layout.fillWidth: true
                                                        Layout.minimumWidth: 0
                                                        spacing: Kirigami.Units.smallSpacing
                                                        Label { text: bound.modelData === "Min" ? I18n.i18n("Minimum") : I18n.i18n("Maximum") }
                                                        TextField {
                                                            objectName: "chartBound-" + chartEntry.modelData.key + "-" + bound.option
                                                            Layout.fillWidth: true
                                                            Layout.minimumWidth: 0
                                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                                                            text: chartEntry.modelData[bound.option] === undefined ? "" : String(chartEntry.modelData[bound.option])
                                                            placeholderText: I18n.i18n("Automatic")
                                                            validator: DoubleValidator { locale: "C" }
                                                            Accessible.name: axis.Kirigami.FormData.label + " · " + (bound.modelData === "Min" ? I18n.i18n("Minimum") : I18n.i18n("Maximum"))
                                                            onEditingFinished: {
                                                                if (!text.length || acceptableInput) {
                                                                    const patch = {}; patch[bound.option] = text.length ? Number(text) : null;
                                                                    UserPreferences.setChartOptions(chartEntry.modelData.key, patch, root.profileId);
                                                                }
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
    }
}
