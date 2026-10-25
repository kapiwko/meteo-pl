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
    property real viewportStart: weather.firstTimestamp
    property real viewportEnd: weather.lastTimestamp
    property real documentFirst: 0
    property real documentLast: 0
    property real selectedTime: 0
    property string selectedGroup: ""
    property bool pinned: false
    property Repeater chartRepeater: null
    property var exportItems: []
    property var exportCandidates: []
    property var exportSelectedKeys: []
    property bool exportSelectionInitialized: false
    property string exportTitle: ""
    property string exportMessage: ""
    property bool exportFailed: false
    property url exportedFile
    readonly property real minimumViewportSpan: Math.min(24 * 3600, Math.max(0, weather.lastTimestamp - weather.firstTimestamp))
    signal configureRequested()
    signal exportReported()
    signal locationSelected(string name, real latitude, real longitude)
    readonly property var selectedChart: visibleCharts.find(chart => chart.key === selectedGroup)
    readonly property var selectedFieldKeys: {
        const keys = [];
        if (selectedChart) for (const field of selectedChart.fields) { keys.push(field.key); if (field.lowerKey) keys.push(field.lowerKey); }
        return keys;
    }
    readonly property var allDetails: selectedTime > 0 ? weather.detailsAt(selectedTime).map(detail => {
        const style = selectedChart ? selectedChart.fields.find(field => field.key === detail.key || field.lowerKey === detail.key) : null;
        return style ? Object.assign({}, detail, {color: style.color, darkColor: style.darkColor}) : detail;
    }) : []
    readonly property bool allProbeParameters: UserPreferences.probeMode === "all" || selectedGroup === ""
    readonly property var inspectorDetails: allProbeParameters ? allDetails : allDetails.filter(detail => selectedFieldKeys.indexOf(detail.key) >= 0)
    readonly property var visibleCharts: {
        if (!weather.hasData || !Object.keys(weather.series).length) return [];
        return weather.configuredCharts(UserPreferences.chartLayout);
    }

    function openSettings() { settingsDialog.open(); }
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
        exportItems = exportCandidates.filter(entry => exportSelectedKeys.indexOf(entry.key) >= 0).map(entry => entry.item);
        if (!exportItems.length) return;
        exportTitle = exportItems.length === 1 ? exportItems[0].title : "Meteogram";
        chartFileDialog.title = "Zapisz wybrane wykresy";
        chartFileDialog.selectedFile = chartExporter.suggestedFile(weather.locationName, exportItems.length === 1 ? exportTitle : "", weather.formatTime(viewportStart, "yyyyMMdd-HHmm"));
        chartFileDialog.open();
    }
    function exportToFile(file, items, title) {
        return chartExporter.savePng(file, items, {
            title: weather.locationName + " · " + title,
            details: weather.modelName + " · obliczenia: " + weather.runText + "\n"
                + weather.formatTime(viewportStart, "d MMM yyyy HH:mm") + " – " + weather.formatTime(viewportEnd, "d MMM yyyy HH:mm")
                + (weather.utc ? " UTC" : " · czas lokalny") + "\n"
                + weather.latitude.toFixed(4) + "°, " + weather.longitude.toFixed(4) + "°",
            source: "ICM UW / meteo.pl · https://www.meteo.pl/ · UM: Met Office · pobrano: " + weather.updatedText,
            background: Kirigami.Theme.backgroundColor, foreground: Kirigami.Theme.textColor
        });
    }
    ChartExporter {
        id: chartExporter
        objectName: "chartExporter"
        onSaved: (file, path) => { root.exportFailed = false; root.exportedFile = file; root.exportMessage = "Zapisano wybrane wykresy."; root.exportItems = []; root.exportReported(); ExportNotifications.saved(file, "Wyeksportowano wykresy"); }
        onFailed: message => { root.exportFailed = true; root.exportMessage = message; root.exportItems = []; root.exportReported(); }
    }
    Dialogs.FileDialog {
        id: chartFileDialog
        objectName: "chartExportDialog"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["Obraz PNG (*.png)"]
        defaultSuffix: "png"
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: root.exportToFile(selectedFile, root.exportItems, root.exportTitle)
        onRejected: root.exportItems = []
    }
    Dialog {
        id: exportSelection
        objectName: "chartExportSelection"
        title: "Eksport wykresów"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, parent ? parent.width - 24 : 460)
        height: Math.min(540, parent ? parent.height - 24 : 540)
        padding: 16
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Save).text = "Zapisz PNG…";
            standardButton(Dialog.Save).enabled = Qt.binding(() => root.exportSelectedKeys.length > 0);
        }
        onAccepted: root.saveSelectedCharts()
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: "Wybierz wykresy do wspólnego obrazu PNG."; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label {
                text: root.weather.formatTime(root.viewportStart, "d MMM HH:mm") + " – " + root.weather.formatTime(root.viewportEnd, "d MMM HH:mm") + (root.weather.utc ? " UTC" : " · czas lokalny")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pixelSize: 11
            }
            RowLayout {
                Button { text: "Wszystkie"; onClicked: root.exportSelectedKeys = root.exportCandidates.map(entry => entry.key) }
                Button { text: "Żaden"; onClicked: root.exportSelectedKeys = [] }
                Label { text: "Wybrano: " + root.exportSelectedKeys.length; opacity: 0.7; font.pixelSize: 11 }
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
            Label { text: "Zachowany zostanie wygląd wykresów, widoczność serii i legend."; Layout.fillWidth: true; wrapMode: Text.WordWrap; opacity: 0.7; font.pixelSize: 11 }
        }
    }
    function number(value, decimals) { return value === null || value === undefined ? "—" : Number(value).toLocaleString(Qt.locale(), 'f', decimals); }
    function resetRange() { viewportStart = weather.firstTimestamp; viewportEnd = weather.lastTimestamp; }
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
        if (pinned || (!force && UserPreferences.probeMode === "off")) return;
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
            }
            ForecastTab { page: 0; text: "Meteogram"; icon.name: "office-chart-line" }
            ForecastTab { page: 1; text: "Godzinowo"; icon.name: "chronometer" }
            ForecastTab { page: 2; text: "Dni"; icon.name: "view-calendar" }
            ForecastTab { page: 3; text: "Mapy"; icon.name: "globe" }
            ForecastTab { page: 4; text: "Synoptyk"; icon.name: "dialog-messages" }
            ForecastTab { page: 5; text: "Artykuły"; icon.name: "view-list-text" }
        }
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: root.viewMode === 3 ? mapsComponent : root.viewMode >= 4 ? contentComponent : forecastComponent
        }
    }
    Component {
        id: forecastComponent
        Pane {
            padding: Kirigami.Units.largeSpacing
            background: null
            contentItem: ColumnLayout {
            spacing: 10
            ForecastHeader { Layout.fillWidth: true; weather: root.weather }
            Kirigami.InlineMessage { visible: root.weather.error.length > 0; text: root.weather.error; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: root.weather.archive; text: "Archiwalne uruchomienie modelu. Daty i godziny wykresu odnoszą się do wybranej prognozy."; type: Kirigami.MessageType.Information; Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: root.weather.stale; text: "Prognoza może być nieaktualna. Ostatnie pobranie: " + root.weather.updatedText; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: root.weather.busy; visible: running && !root.weather.hasData }
            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                visible: !root.weather.hasData && !root.weather.busy
                icon.name: "weather-none-available"
                text: "Brak prognozy"
                explanation: "Wybierz miejscowość i pobierz meteogram."
                helpfulAction: Kirigami.Action { text: "Wybierz lokalizację"; onTriggered: root.configureRequested() }
            }
            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: root.viewMode === 0 ? chartsComponent : root.viewMode === 1 ? hourlyComponent : dailyComponent
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
                        columns: width < 680 ? 1 : 2
                        columnSpacing: 12
                        rowSpacing: 0
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            ToolButton {
                                objectName: "earlierTimeButton"
                                icon.name: "go-previous"
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial && root.viewportStart > root.weather.firstTimestamp + 1
                                Accessible.name: "Wcześniejszy przedział"
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
                                ToolTip.text: "Widoczny przedział · " + (root.weather.utc ? "UTC" : "czas lokalny")
                                ToolTip.visible: dateHover.hovered
                                HoverHandler { id: dateHover }
                            }
                            ToolButton {
                                icon.name: "go-next"
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial && root.viewportEnd < root.weather.lastTimestamp - 1
                                Accessible.name: "Późniejszy przedział"
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.moveRange(1)
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            ToolButton {
                                icon.name: "zoom-out"
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial
                                Accessible.name: "Pokaż dłuższy przedział"
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.zoom(1.5, (root.viewportStart + root.viewportEnd) / 2)
                            }
                            Slider {
                                id: zoomSlider
                                objectName: "chartZoomSlider"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 70
                                Layout.preferredWidth: 120
                                Layout.maximumWidth: 180
                                implicitHeight: 28
                                from: 1
                                to: Math.max(1, chartStatus.fullSpan / Math.max(1, root.minimumViewportSpan))
                                value: chartStatus.fullSpan / chartStatus.span
                                enabled: to > from
                                Accessible.name: "Powiększenie wykresów"
                                ToolTip.text: Math.round(value * 100) + "% · " + Math.round(chartStatus.span / 3600) + " godzin"
                                ToolTip.visible: hovered || pressed
                                onMoved: root.zoom(chartStatus.fullSpan / value / chartStatus.span, (root.viewportStart + root.viewportEnd) / 2)
                            }
                            ToolButton {
                                icon.name: "zoom-in"
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.span > root.minimumViewportSpan + 1
                                Accessible.name: "Pokaż krótszy przedział"
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.zoom(0.67, (root.viewportStart + root.viewportEnd) / 2)
                            }
                            ToolButton {
                                icon.name: "zoom-fit-best"
                                implicitWidth: 28; implicitHeight: 28
                                enabled: chartStatus.partial
                                Accessible.name: "Cała prognoza"
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.resetRange()
                            }
                            ToolButton {
                                text: chartExporter.busy ? "Eksportuję…" : "Eksportuj…"
                                icon.name: "document-export"
                                enabled: !chartExporter.busy && root.visibleCharts.length > 0
                                Accessible.name: "Eksportuj wybrane wykresy do PNG"
                                ToolTip.text: Accessible.name; ToolTip.visible: hovered
                                onClicked: root.chooseExport()
                            }
                        }
                    }
                    Slider {
                        objectName: "chartTimeSlider"
                        Layout.fillWidth: true
                        visible: chartStatus.partial
                        implicitHeight: 22
                        from: root.weather.firstTimestamp
                        to: Math.max(from, root.weather.lastTimestamp - chartStatus.span)
                        value: root.viewportStart
                        Accessible.name: "Przesuń widoczny przedział czasu"
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
                    Kirigami.Action { text: "Otwórz"; icon.name: "document-open"; visible: !root.exportFailed; onTriggered: ExportNotifications.openFile(root.exportedFile) },
                    Kirigami.Action { text: "Otwórz folder"; icon.name: "folder-open"; visible: !root.exportFailed; onTriggered: ExportNotifications.openFolder(root.exportedFile) }
                ]
            }

        }
        }
    }
    Component {
        id: chartsComponent
        ScrollView {
            id: chartsScroll
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
                        nights: UserPreferences.showNight ? root.weather.nightIntervals : []
                        onProbeMoved: (timestamp, position) => root.showProbe(timestamp, position, groupKey)
                        onProbeLeft: closeProbe.restart()
                        onPinRequested: (timestamp, position) => {
                            if (root.pinned) root.pinned = false;
                            else { root.showProbe(timestamp, position, groupKey, true); root.pinned = true; }
                        }
                        onZoomRequested: (factor, center) => root.zoom(factor, center)
                    }
                }
            }
        }
    }
    Component {
        id: hourlyComponent
        ScrollView {
            id: hourScroll
            clip: true
            contentWidth: Math.max(availableWidth, 890)
            Column {
                width: hourScroll.contentWidth
                Row {
                    Repeater {
                        model: ["Godzina", "Temp.", "Odczuw.", "Opad mm", "Szansa %", "Wiatr " + root.weather.windUnit, "Porywy", "hPa", "Chmury %"]
                        delegate: Label { required property string modelData; width: modelData === "Godzina" ? 122 : 95; text: modelData; font.bold: true; padding: 8 }
                    }
                }
                Repeater {
                    model: root.weather.hourlyForecast
                    delegate: ItemDelegate {
                        id: hourRow
                        required property var modelData
                        required property int index
                        width: parent.width
                        implicitHeight: 38
                        onClicked: { root.pinned = false; root.showProbe(modelData.time, mapToItem(null, width/2, 0), "", true); root.pinned = true; }
                        background: Rectangle { color: hourRow.index % 2 ? Kirigami.Theme.alternateBackgroundColor : "transparent" }
                        contentItem: Row {
                            Label { width: 110; text: root.weather.formatTime(hourRow.modelData.time, "ddd HH:mm"); font.bold: true }
                            Repeater {
                                model: ["airtmp_point", "wchill_point", "pcpttl_aver", "pcpttlprob_point", "wind10_sd_true_prev_point", "wind_gust_max", "slpres_point", "cldtot_aver"]
                                delegate: Label { required property string modelData; width: 95; text: root.number(hourRow.modelData[modelData], ["pcpttl_aver"].indexOf(modelData) >= 0 ? 1 : 0) }
                            }
                        }
                    }
                }
            }
        }
    }
    Component {
        id: dailyComponent
        ScrollView {
            id: dailyScroll
            implicitWidth: 0
            contentWidth: availableWidth
            clip: true
            GridLayout {
                id: dayGrid
                width: dailyScroll.availableWidth
                columns: Math.max(1, Math.min(days.count, Math.floor(width / 160)))
                readonly property real cardWidth: (width - columnSpacing * (columns - 1)) / columns
                Repeater {
                    id: days
                    model: root.weather.dailyForecast
                    delegate: Frame {
                        id: dayCard
                        objectName: "forecastDayCard"
                        required property var modelData
                        Layout.minimumWidth: dayGrid.cardWidth
                        Layout.preferredWidth: dayGrid.cardWidth
                        Layout.maximumWidth: dayGrid.cardWidth
                        Layout.fillHeight: true
                        padding: 12
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            Label { Layout.fillWidth: true; Layout.preferredHeight: 28; text: dayCard.modelData.day; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap }
                            Kirigami.Icon { source: dayCard.modelData.icon; Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignHCenter; Accessible.name: dayCard.modelData.description }
                            Label { Layout.fillWidth: true; Layout.preferredHeight: 32; text: dayCard.modelData.description; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; opacity: 0.7; font.pixelSize: 11 }
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                Label { text: root.number(dayCard.modelData.maximum, 0) + "°"; font.pixelSize: 20; font.bold: true }
                                Label { text: root.number(dayCard.modelData.minimum, 0) + "°"; font.pixelSize: 16; opacity: 0.65 }
                            }
                            Label { Layout.fillWidth: true; text: "Opad: " + root.number(dayCard.modelData.rain, 1) + " mm"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11 }
                            Label { Layout.fillWidth: true; text: "Porywy: " + root.number(dayCard.modelData.wind, 0) + " " + root.weather.windUnit; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11 }
                        }
                    }
                }
            }
        }
    }
    Component { id: mapsComponent; Pane { padding: Kirigami.Units.largeSpacing; background: null; contentItem: WeatherMap { weather: root.weather; onLocationSelected: (name, latitude, longitude) => { root.locationSelected(name, latitude, longitude); root.viewMode = 0; } } } }
    Component { id: contentComponent; Pane { padding: Kirigami.Units.largeSpacing; background: null; contentItem: EditorialView { commentary: root.viewMode === 4 } } }
    Connections {
        target: UserPreferences
        function onDisplayChanged() { if (UserPreferences.probeMode === "off" && !root.pinned) inspector.close(); }
    }
    onViewModeChanged: inspector.close()
    Timer { id: closeProbe; interval: 220; onTriggered: if (!root.pinned && !inspector.hovered) inspector.close() }
    ForecastInspector {
        id: inspector
        weather: root.weather
        timestamp: root.selectedTime
        pinned: root.pinned
        allParameters: root.allProbeParameters
        details: root.inspectorDetails
        highlightedKeys: root.selectedFieldKeys
        onPinToggled: root.pinned = !root.pinned
        onPointerLeft: closeProbe.restart()
        onClosed: { root.selectedTime = 0; root.pinned = false; }
    }
    MeteoSettings {
        id: settingsDialog
        weather: root.weather
        showTimeRange: root.viewMode === 0
        startTime: root.viewportStart
        endTime: root.viewportEnd
        onHoursSelected: hours => root.setHours(hours)
    }
}
