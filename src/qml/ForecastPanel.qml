pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import QtCore
import org.kde.kirigami as Kirigami

Item {
    id: root
    required property WeatherModel weather
    property int viewMode: 0
    property bool touchMode: Kirigami.Settings.isMobile
    property bool mobileLayout: false
    function navigateBack() {
        if (viewMode < 4) return false;
        const page = mainViewLoader.item as Pane;
        const reader = page ? page.contentItem as EditorialView : null;
        if (!reader || !reader.selectedArticle) return false;
        reader.selectedArticle = null;
        return true;
    }
    property real viewportStart: weather.firstTimestamp
    property real viewportEnd: weather.lastTimestamp
    property real documentFirst: 0
    property real documentLast: 0
    property real selectedTime: 0
    property string selectedGroup: ""
    property bool pinned: false
    property bool identificationEnabled: false
    onIdentificationEnabledChanged: if (!identificationEnabled) inspector.close()
    property Repeater chartRepeater: null
    property var exportItems: []
    property var exportCandidates: []
    property var exportSelectedKeys: []
    property bool exportSelectionInitialized: false
    property string exportTitle: ""
    property string exportMessage: ""
    property bool exportFailed: false
    property bool exportingReadings: false
    property url exportedFile
    readonly property real minimumViewportSpan: Math.min(24 * 3600, Math.max(0, weather.lastTimestamp - weather.firstTimestamp))
    signal configureRequested()
    signal addLocationRequested()
    signal settingsRequested()
    signal exportReported()
    signal locationSelected(string name, real latitude, real longitude)
    readonly property var selectedChart: visibleCharts.find(chart => chart.key === selectedGroup)
    readonly property var selectedFieldKeys: {
        const keys = [];
        if (selectedChart) for (const field of selectedChart.fields) { keys.push(field.key); if (field.lowerKey) keys.push(field.lowerKey); }
        return keys;
    }
    readonly property var allDetails: {
        const revision = weather.dataRevision;
        return selectedTime > 0 ? weather.detailsAt(selectedTime).map(detail => {
        const style = selectedChart ? selectedChart.fields.find(field => field.key === detail.key || field.lowerKey === detail.key) : null;
        return style ? Object.assign({}, detail, {color: style.color, darkColor: style.darkColor}) : detail;
        }) : [];
    }
    readonly property bool allProbeParameters: UserPreferences.probeMode === "all" || selectedGroup === ""
    readonly property var inspectorDetails: allProbeParameters ? allDetails : allDetails.filter(detail => selectedFieldKeys.indexOf(detail.key) >= 0)
    readonly property var visibleCharts: {
        if (!weather.hasData || !Object.keys(weather.series).length) return [];
        return weather.configuredCharts(UserPreferences.chartLayout);
    }

    function openSettings() {
        if (mobileLayout) settingsRequested();
        else if (settingsDialog.item) settingsDialog.item.open();
    }
    function chooseExport() {
        if (chartExporter.busy || !chartRepeater) return;
        const items = [];
        for (let i = 0; i < chartRepeater.count; ++i) {
            const item = chartRepeater.itemAt(i);
            if (item) items.push({key: item.groupKey, title: item.title, item: item});
        }
        if (!items.length) return;
        exportCandidates = items;
        if (!exportSelectionInitialized) { exportSelectedKeys = items.map(item => item.key); exportSelectionInitialized = true; }
        else exportSelectedKeys = exportSelectedKeys.filter(key => items.some(item => item.key === key));
        exportSelection.open();
    }
    function saveSelectedCharts() {
        saveCharts(exportCandidates.filter(entry => exportSelectedKeys.indexOf(entry.key) >= 0).map(entry => entry.item));
    }
    function saveCharts(items) {
        if (chartExporter.busy || !items.length) return;
        exportItems = items;
        exportTitle = exportItems.length === 1 ? exportItems[0].title : I18n.i18n("Meteogram");
        chartFileDialog.title = exportItems.length === 1 ? I18n.i18n("Save chart as…") : I18n.i18n("Save selected charts");
        chartFileDialog.selectedFile = chartExporter.suggestedFile(weather.locationName, exportItems.length === 1 ? exportTitle : "", weather.formatTime(viewportStart, "yyyyMMdd-HHmm"));
        chartFileDialog.open();
    }
    function exportToFile(file, items, title) {
        exportingReadings = false;
        const metadata = weather.chartExportMetadata(title, viewportStart, viewportEnd);
        metadata.background = Kirigami.Theme.backgroundColor;
        metadata.foreground = Kirigami.Theme.textColor;
        return chartExporter.savePng(file, items, metadata);
    }

    function reportExport(file, error, readings) {
        exportFailed = error.length > 0;
        if (!exportFailed) exportedFile = file;
        exportMessage = exportFailed ? error : readings ? I18n.i18n("Parameter reading saved.") : I18n.i18n("Selected charts saved.");
        exportItems = [];
        exportingReadings = false;
        exportReported();
        if (!exportFailed) ExportNotifications.saved(file, readings ? I18n.i18n("Reading exported") : I18n.i18n("Charts exported"));
    }
    function chooseReadingExport(format) {
        if (chartExporter.busy || selectedTime <= 0 || !inspector.visibleDetails.length) return;
        pinned = true;
        closeProbe.stop();
        readingFileDialog.format = format;
        const file = chartExporter.suggestedFile(weather.locationName, "odczyt", weather.formatTime(selectedTime, "yyyyMMdd-HHmm"));
        readingFileDialog.selectedFile = format === "csv" ? file.toString().replace(/\.png$/i, ".csv") : file;
        readingFileDialog.open();
    }
    function exportReadingsToFile(file, format) {
        if (chartExporter.busy) return false;
        if (selectedTime <= 0 || !inspector.visibleDetails.length || !inspector.opened) {
            reportExport(file, I18n.i18n("Open a parameter reading and try again."), true);
            return false;
        }
        pinned = true;
        closeProbe.stop();
        const scope = allProbeParameters ? (inspector.categories.find(entry => entry.key === inspector.category) || {name: I18n.i18n("All")}).name : selectedChart ? selectedChart.title : I18n.i18n("Parameters");
        const source = "ICM UW / meteo.pl · https://www.meteo.pl/ · UM: Met Office";
        if (format === "csv") {
            const result = readingsExporter.saveCsv(file, inspector.visibleDetails, {
                timestamp: selectedTime, utc: weather.utc, location: weather.locationName,
                model: weather.modelName, run: weather.runText, scope: scope, source: source
            });
            reportExport(result.ok ? result.file : file, result.ok ? "" : result.error, true);
            return result.ok;
        }
        if (format !== "png") return false;
        exportingReadings = true;
        return chartExporter.savePng(file, [inspector.readingsItem], {
            title: weather.locationName + I18n.i18n(" · parameter reading · ") + scope,
            details: weather.formatTime(selectedTime, "ddd d MMM yyyy · HH:mm t") + "\n" + weather.modelName + I18n.i18n(" · model run: ") + weather.runText,
            source: source, background: Kirigami.Theme.backgroundColor, foreground: Kirigami.Theme.textColor
        });
    }
    ReadingsExporter { id: readingsExporter }
    Dialogs.FileDialog {
        id: readingFileDialog
        objectName: "readingExportDialog"
        property string format: "png"
        title: format === "csv" ? I18n.i18n("Save parameter reading as CSV") : I18n.i18n("Save parameter reading as PNG")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: format === "csv" ? [I18n.i18n("CSV data (*.csv)")] : [I18n.i18n("PNG image (*.png)")]
        defaultSuffix: format
        currentFolder: StandardPaths.writableLocation(format === "csv" ? StandardPaths.DocumentsLocation : StandardPaths.PicturesLocation)
        onAccepted: root.exportReadingsToFile(selectedFile, format)
    }
    ChartExporter {
        id: chartExporter
        objectName: "chartExporter"
        onSaved: (file, path) => root.reportExport(file, "", root.exportingReadings)
        onFailed: message => root.reportExport(root.exportedFile, message, root.exportingReadings)
    }
    Dialogs.FileDialog {
        id: chartFileDialog
        objectName: "chartExportDialog"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: [I18n.i18n("PNG image (*.png)")]
        defaultSuffix: "png"
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: root.exportToFile(selectedFile, root.exportItems, root.exportTitle)
        onRejected: root.exportItems = []
    }
    Dialog {
        id: exportSelection
        objectName: "chartExportSelection"
        title: I18n.i18n("Export charts")
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, parent ? parent.width - 24 : 460)
        height: Math.min(540, parent ? parent.height - 24 : 540)
        padding: 16
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Save).text = I18n.i18n("Save PNG…");
            standardButton(Dialog.Save).enabled = Qt.binding(() => root.exportSelectedKeys.length > 0);
        }
        onAccepted: root.saveSelectedCharts()
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: I18n.i18n("Choose charts to combine into one PNG image."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label {
                text: root.weather.formatTime(root.viewportStart, "d MMM HH:mm") + " – " + root.weather.formatTime(root.viewportEnd, "d MMM HH:mm") + (root.weather.utc ? " UTC" : I18n.i18n(" · local time"))
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pixelSize: 11
            }
            RowLayout {
                Button { text: I18n.i18n("All"); onClicked: root.exportSelectedKeys = root.exportCandidates.map(entry => entry.key) }
                Button { text: I18n.i18n("Deselect all"); onClicked: root.exportSelectedKeys = [] }
                Label { text: I18n.i18np("%1 chart selected", "%1 charts selected", root.exportSelectedKeys.length); opacity: 0.7; font.pixelSize: 11 }
            }
            ScrollView {
                id: exportScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: exportScroll.availableWidth
                    Repeater {
                        model: root.exportCandidates
                        delegate: CheckBox {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.title
                            checked: root.exportSelectedKeys.indexOf(modelData.key) >= 0
                            onToggled: {
                                const keys = root.exportSelectedKeys.filter(key => key !== modelData.key);
                                if (checked) keys.push(modelData.key);
                                root.exportSelectedKeys = keys;
                            }
                        }
                    }
                }
            }
            Label { text: I18n.i18n("Chart appearance, series visibility and legends will be preserved."); Layout.fillWidth: true; wrapMode: Text.WordWrap; opacity: 0.7; font.pixelSize: 11 }
        }
    }
    function number(value, decimals) { return value === null || value === undefined ? "—" : Number(value).toLocaleString(Qt.locale(), 'f', decimals); }
    function resetRange() { viewportStart = weather.firstTimestamp; viewportEnd = weather.lastTimestamp; }
    function setRange(first, last) {
        const span = Math.min(weather.lastTimestamp - weather.firstTimestamp, Math.max(minimumViewportSpan, last - first));
        viewportStart = Math.max(weather.firstTimestamp, Math.min(weather.lastTimestamp - span, first));
        viewportEnd = viewportStart + span;
    }
    function setHours(hours) {
        if (hours === 0) { resetRange(); return; }
        const duration = Math.min(hours * 3600, weather.lastTimestamp - weather.firstTimestamp);
        viewportStart = Math.max(weather.firstTimestamp, Math.min(Date.now()/1000, weather.lastTimestamp - duration));
        viewportEnd = viewportStart + duration;
    }
    function zoom(factor, center) {
        const span = Math.min(weather.lastTimestamp - weather.firstTimestamp, Math.max(minimumViewportSpan, (viewportEnd - viewportStart) * factor));
        const fraction = (center - viewportStart) / Math.max(1, viewportEnd - viewportStart);
        viewportStart = Math.max(weather.firstTimestamp, Math.min(center - span * fraction, weather.lastTimestamp - span));
        viewportEnd = viewportStart + span;
    }
    function moveRange(delta) {
        const span = viewportEnd - viewportStart;
        viewportStart = Math.max(weather.firstTimestamp, Math.min(weather.lastTimestamp - span, viewportStart + delta * span / 2));
        viewportEnd = viewportStart + span;
    }
    function showProbe(time, position, group, force) {
        if (pinned || (!force && !identificationEnabled)) return;
        closeProbe.stop(); selectedTime = time; selectedGroup = group;
        if (inspector.parent) {
            const fitsRight = inspector.parent.width - position.x >= inspector.width + 24;
            const idealX = fitsRight ? position.x + 16 : position.x - inspector.width - 16;
            inspector.x = Math.max(8, Math.min(inspector.parent.width - inspector.width - 8, idealX));
            let idealY = position.y - 40;
            if (position.x >= inspector.x && position.x <= inspector.x + inspector.width)
                idealY = inspector.parent.height - position.y >= inspector.height + 24 ? position.y + 16 : position.y - inspector.height - 16;
            inspector.y = Math.max(8, Math.min(inspector.parent.height - inspector.height - 8, idealY));
        }
        if (!inspector.opened) inspector.open();
    }
    Connections {
        target: root.weather
        function onStateChanged() {
            if (root.documentFirst !== root.weather.firstTimestamp || root.documentLast !== root.weather.lastTimestamp) {
                root.documentFirst = root.weather.firstTimestamp; root.documentLast = root.weather.lastTimestamp;
                root.resetRange(); inspector.close();
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Kirigami.NavigationTabBar {
            id: navigation
            objectName: "forecastNavigation"
            visible: !root.mobileLayout
            Layout.fillWidth: true
            maximumContentWidth: root.width
            spacing: Kirigami.Units.smallSpacing
            buttonWidth: Math.max(0, (root.width - 5 * spacing) / 6)
            currentIndex: root.viewMode
            onCurrentIndexChanged: if (currentIndex >= 0) root.viewMode = currentIndex
            function selectPage() {
                const button = tabGroup.buttons[root.viewMode];
                if (button && !button.checked) button.checked = true;
            }
            Component.onCompleted: Qt.callLater(selectPage)
            Connections { target: root; function onViewModeChanged() { navigation.selectPage(); } }
            component ForecastTab: Kirigami.NavigationTabButton {
                id: forecastTab
                required property int page
                Layout.minimumWidth: navigation.buttonWidth
                Layout.maximumWidth: navigation.buttonWidth
                Layout.fillHeight: true
                ButtonGroup.group: navigation.tabGroup
                checkable: true
                checked: root.viewMode === page
                onClicked: root.viewMode = page
                display: root.width < 720 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                Accessible.name: text
                ToolTip.text: text
                ToolTip.visible: hovered
                implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, Kirigami.Units.gridUnit * 2) + topInset + bottomInset
                contentItem: Item {
                    implicitWidth: tabContents.implicitWidth
                    implicitHeight: tabContents.implicitHeight
                    RowLayout {
                        id: tabContents
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing * 1.5
                        Kirigami.Icon {
                            source: forecastTab.icon.name
                            color: forecastTab.icon.color
                            Layout.preferredWidth: forecastTab.icon.width
                            Layout.preferredHeight: forecastTab.icon.height
                            Accessible.ignored: true
                        }
                        Label {
                            text: forecastTab.text
                            font: forecastTab.font
                            visible: forecastTab.display !== AbstractButton.IconOnly
                            color: Kirigami.Theme.textColor
                            Accessible.ignored: true
                        }
                    }
                }
            }
            ForecastTab { page: 0; text: I18n.i18n("Meteogram"); icon.name: "office-chart-line" }
            ForecastTab { page: 1; text: I18n.i18n("Hourly"); icon.name: "chronometer" }
            ForecastTab { page: 2; text: I18n.i18n("Days"); icon.name: "view-calendar" }
            ForecastTab { page: 3; text: I18n.i18n("Maps"); icon.name: "globe" }
            ForecastTab { page: 4; text: I18n.i18n("Synoptic"); icon.name: "dialog-messages" }
            ForecastTab { page: 5; text: I18n.i18n("Articles"); icon.name: "view-list-text" }
        }
        Loader {
            id: mainViewLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: root.viewMode === 3 ? mapsComponent : root.viewMode >= 4 ? contentComponent : forecastComponent
        }
    }
    Component {
        id: forecastComponent
        Pane {
            padding: Kirigami.Units.largeSpacing
            leftPadding: root.mobileLayout ? Kirigami.Units.smallSpacing : padding
            rightPadding: root.mobileLayout ? Kirigami.Units.smallSpacing : padding
            background: null
            contentItem: ColumnLayout {
            spacing: 10
            ForecastHeader {
                id: forecastHeader
                visible: !root.mobileLayout
                Layout.fillWidth: true
                weather: root.weather
                onAddLocationRequested: root.addLocationRequested()
                onLocationSelected: (name, latitude, longitude) => root.locationSelected(name, latitude, longitude)
            }
            Loader {
                id: mobileHeader
                Layout.fillWidth: true
                active: root.mobileLayout
                visible: active
                sourceComponent: CompactForecast { weather: root.weather; showDays: false; showPlacePicker: false }
            }
            ProfileUpdateNotice { Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: !root.mobileLayout && root.weather.error.length > 0; text: root.weather.error; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: root.weather.archive; text: I18n.i18n("Archived model run. Chart dates and times refer to the selected forecast."); type: Kirigami.MessageType.Information; Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: root.weather.stale; text: I18n.i18n("The forecast may be outdated. Last downloaded: ") + root.weather.updatedText; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                visible: !root.weather.hasData && !(root.mobileLayout ? mobileHeader.item && mobileHeader.item.loading : forecastHeader.loading)
                icon.name: "mark-location"
                text: I18n.i18n("No forecast")
                explanation: I18n.i18n("Choose a place to download its meteogram.")
                helpfulAction: Kirigami.Action { text: I18n.i18n("Choose location"); onTriggered: root.configureRequested() }
            }
            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: root.viewMode === 0 ? chartsComponent : root.viewMode === 1 ? (root.mobileLayout ? mobileHourlyComponent : hourlyComponent) : dailyComponent
            }
            Frame {
                id: chartStatus
                objectName: "chartStatusBar"
                visible: root.viewMode === 0 && root.weather.hasData
                Layout.fillWidth: true
                padding: 4
                readonly property real fullSpan: Math.max(1, root.weather.lastTimestamp - root.weather.firstTimestamp)
                readonly property real span: Math.max(1, root.viewportEnd - root.viewportStart)
                readonly property bool partial: span < fullSpan - 1
                background: Rectangle {
                    color: "transparent"
                    Rectangle { width: parent.width; height: 1; color: Kirigami.Theme.textColor; opacity: 0.12 }
                }
                contentItem: ColumnLayout {
                    spacing: 0
                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.touchMode || width >= 680 ? 2 : 1
                        columnSpacing: 12
                        rowSpacing: 0
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            ToolButton {
                                objectName: "earlierTimeButton"
                                visible: !root.touchMode
                                icon.name: "go-previous"
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial && root.viewportStart > root.weather.firstTimestamp + 1
                                Accessible.name: I18n.i18n("Earlier interval")
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.moveRange(-1)
                            }
                            Label {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 100
                                text: root.weather.formatTime(root.viewportStart, "d MMM HH:mm") + " – " + root.weather.formatTime(root.viewportEnd, "d MMM HH:mm") + (root.weather.utc ? " UTC" : "")
                                font.pixelSize: 10
                                opacity: 0.7
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                ToolTip.text: I18n.i18n("Visible interval · ") + (root.weather.utc ? "UTC" : I18n.i18n("local time"))
                                ToolTip.visible: dateHover.hovered
                                HoverHandler { id: dateHover }
                            }
                            ToolButton {
                                icon.name: "go-next"
                                visible: !root.touchMode
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial && root.viewportEnd < root.weather.lastTimestamp - 1
                                Accessible.name: I18n.i18n("Later interval")
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.moveRange(1)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            ToolButton {
                                icon.name: "zoom-out"
                                visible: !root.touchMode
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial
                                Accessible.name: I18n.i18n("Show a longer interval")
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.zoom(1.5, (root.viewportStart + root.viewportEnd) / 2)
                            }
                            Slider {
                                id: zoomSlider
                                objectName: "chartZoomSlider"
                                visible: !root.touchMode
                                Layout.fillWidth: true
                                Layout.minimumWidth: 70
                                Layout.preferredWidth: 120
                                Layout.maximumWidth: 180
                                implicitHeight: 28
                                from: 1
                                to: Math.max(1, chartStatus.fullSpan / Math.max(1, root.minimumViewportSpan))
                                value: chartStatus.fullSpan / chartStatus.span
                                enabled: to > from
                                Accessible.name: I18n.i18n("Chart zoom")
                                ToolTip.text: Math.round(value * 100) + "% · " + Math.round(chartStatus.span / 3600) + I18n.i18n(" hours")
                                ToolTip.visible: hovered || pressed
                                onMoved: root.zoom(chartStatus.fullSpan / value / chartStatus.span, (root.viewportStart + root.viewportEnd) / 2)
                            }
                            ToolButton {
                                icon.name: "zoom-in"
                                visible: !root.touchMode
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.span > root.minimumViewportSpan + 1
                                Accessible.name: I18n.i18n("Show a shorter interval")
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.zoom(0.67, (root.viewportStart + root.viewportEnd) / 2)
                            }
                            ToolButton {
                                icon.name: "zoom-fit-best"
                                visible: !root.touchMode
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial
                                Accessible.name: I18n.i18n("Entire forecast")
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.resetRange()
                            }
                            ToolButton {
                                objectName: "identificationButton"
                                icon.name: "color-picker"
                                implicitWidth: root.touchMode ? 44 : 28; implicitHeight: implicitWidth
                                checkable: true
                                checked: root.identificationEnabled
                                onClicked: root.identificationEnabled = checked
                                Accessible.name: I18n.i18n("Parameter identification mode")
                                ToolTip.text: checked ? I18n.i18n("Disable parameter identification") : root.touchMode ? I18n.i18n("Identify parameters · tap a chart") : I18n.i18n("Identify parameters · move the cursor over a chart")
                                ToolTip.visible: hovered
                            }
                            ToolButton {
                                text: chartExporter.busy ? I18n.i18n("Exporting…") : I18n.i18n("Export…")
                                icon.name: "document-export"
                                display: root.touchMode ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                                implicitHeight: root.touchMode ? 44 : implicitContentHeight + topPadding + bottomPadding
                                enabled: !chartExporter.busy && root.visibleCharts.length > 0
                                Accessible.name: I18n.i18n("Export selected charts to PNG")
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.chooseExport()
                            }
                        }
                    }
                    Slider {
                        objectName: "chartTimeSlider"
                        Layout.fillWidth: true
                        visible: chartStatus.partial && !root.touchMode
                        implicitHeight: 22
                        from: root.weather.firstTimestamp
                        to: Math.max(from, root.weather.lastTimestamp - chartStatus.span)
                        value: root.viewportStart
                        Accessible.name: I18n.i18n("Move the visible time interval")
                        onMoved: { const span = chartStatus.span; root.viewportStart = value; root.viewportEnd = value + span; }
                    }

                }
            }
            Kirigami.InlineMessage {
                id: exportNotice
                objectName: "exportNotice"
                Layout.fillWidth: true
                visible: false
                text: root.exportMessage
                type: root.exportFailed ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
                showCloseButton: true
                Connections { target: root; function onExportReported() { exportNotice.visible = true; } }
                actions: [
                    Kirigami.Action { text: I18n.i18n("Open"); icon.name: "document-open"; visible: !root.exportFailed; onTriggered: ExportNotifications.openFile(root.exportedFile) },
                    Kirigami.Action { text: I18n.i18n("Open folder"); icon.name: "folder-open"; visible: !root.exportFailed; onTriggered: ExportNotifications.openFolder(root.exportedFile) }
                ]
            }

        }
        }
    }
    Component {
        id: chartsComponent
        ScrollView {
            id: chartsScroll
            objectName: "meteogramScrollView"
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: chartsScroll.availableWidth
                spacing: 22
                Repeater {
                    id: charts
                    Component.onCompleted: root.chartRepeater = charts
                    model: root.weather.hasData ? root.visibleCharts : []
                    delegate: Meteogram {
                        required property var modelData
                        Layout.fillWidth: true
                        weather: root.weather
                        title: modelData.title
                        groupKey: modelData.key
                        fields: modelData.fields
                        chartConfiguration: modelData
                        startTime: root.viewportStart
                        endTime: root.viewportEnd
                        selectedTime: root.selectedTime
                        identificationEnabled: root.identificationEnabled
                        touchNavigationEnabled: true
                        compactAxes: root.mobileLayout
                        exportHandler: chart => root.saveCharts([chart])
                        nights: UserPreferences.showNight ? root.weather.nightIntervals : []
                        onProbeMoved: (timestamp, position) => root.showProbe(timestamp, position, groupKey)
                        onProbeLeft: closeProbe.restart()
                        onPinRequested: (timestamp, position) => {
                            if (root.pinned) root.pinned = false;
                            else { root.showProbe(timestamp, position, groupKey, true); root.pinned = true; }
                        }
                        onZoomRequested: (factor, center) => root.zoom(factor, center)
                        onRangeRequested: (first, last) => root.setRange(first, last)
                        onResetRangeRequested: root.resetRange()
                    }
                }
            }
        }
    }
    Component {
        id: mobileHourlyComponent
        MobileHourlyForecast {
            weather: root.weather
            onReadingRequested: (timestamp, position) => { root.pinned = false; root.showProbe(timestamp, position, "", true); root.pinned = true; }
        }
    }
    Component {
        id: hourlyComponent
        HourlyForecastTable {
            weather: root.weather
            onReadingRequested: (timestamp, position) => { root.pinned = false; root.showProbe(timestamp, position, "", true); root.pinned = true; }
        }
    }
    Component {
        id: dailyComponent
        DailyForecastView { weather: root.weather }
    }
    Component { id: mapsComponent; Pane { padding: Kirigami.Units.largeSpacing; background: null; contentItem: WeatherMap { weather: root.weather; mobileLayout: root.mobileLayout; onLocationSelected: (name, latitude, longitude) => { root.locationSelected(name, latitude, longitude); root.viewMode = 0; } } } }
    Component { id: contentComponent; Pane { padding: Kirigami.Units.largeSpacing; background: null; contentItem: EditorialView { commentary: root.viewMode === 4; mobileLayout: root.mobileLayout } } }
    onViewModeChanged: { identificationEnabled = false; inspector.close(); }
    Timer { id: closeProbe; interval: 220; onTriggered: if (!root.pinned && !inspector.hovered) inspector.close() }
    ForecastInspector {
        id: inspector
        weather: root.weather
        timestamp: root.selectedTime
        pinned: root.pinned
        allParameters: root.allProbeParameters
        details: root.inspectorDetails
        highlightedKeys: root.selectedFieldKeys
        exportBusy: chartExporter.busy
        onExportRequested: format => root.chooseReadingExport(format)
        onPinToggled: root.pinned = !root.pinned
        onPointerLeft: closeProbe.restart()
        onClosed: { root.selectedTime = 0; root.pinned = false; }
    }
    Loader {
        id: settingsDialog
        active: !root.mobileLayout
        sourceComponent: Component {
            MeteoSettings {
                weather: root.weather
                showTimeRange: root.viewMode === 0
                startTime: root.viewportStart
                endTime: root.viewportEnd
                onHoursSelected: hours => root.setHours(hours)
            }
        }
    }
}
