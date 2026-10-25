pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    property bool originalView: false
    signal configureRequested()
    signal locationSelected(string name, real latitude, real longitude)
    readonly property var allDetails: selectedTime > 0 ? weather.detailsAt(selectedTime) : []
    readonly property var visibleCharts: {
        const catalog = weather.chartGroups;
        const result = [];
        for (const config of UserPreferences.chartLayout) {
            if (!config.enabled) continue;
            const chart = catalog.find(group => group.key === config.key);
            if (!chart) continue;
            result.push({key: chart.key, title: chart.title, fields: chart.fields.filter(field => config.fields.indexOf(field.key) >= 0)});
        }
        return result;
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
        const span = Math.min(weather.lastTimestamp - weather.firstTimestamp, Math.max(6 * 3600, (viewportEnd - viewportStart) * factor));
        const fraction = (center - viewportStart) / Math.max(1, viewportEnd - viewportStart);
        viewportStart = Math.max(weather.firstTimestamp, Math.min(center - span * fraction, weather.lastTimestamp - span));
        viewportEnd = viewportStart + span;
    }
    function moveRange(delta) {
        const span = viewportEnd - viewportStart;
        viewportStart = Math.max(weather.firstTimestamp, Math.min(weather.lastTimestamp - span, viewportStart + delta * span / 2));
        viewportEnd = viewportStart + span;
    }
    function showProbe(time, position, group) {
        if (pinned) return;
        closeProbe.stop(); selectedTime = time; selectedGroup = group;
        if (inspector.parent) {
            const fitsRight = inspector.parent.width - position.x >= inspector.width + 24;
            const idealX = fitsRight ? position.x + 16 : position.x - inspector.width - 16;
            inspector.x = Math.max(8, Math.min(inspector.parent.width - inspector.width - 8, idealX));
            inspector.y = Math.max(8, Math.min(inspector.parent.height - inspector.height - 8, position.y - 40));
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
        spacing: 8
        TabBar {
            id: navigation
            Layout.fillWidth: true
            currentIndex: root.viewMode
            onCurrentIndexChanged: root.viewMode = currentIndex
            TabButton { width: Math.max(90, root.width / 6); text: "Meteogram" }
            TabButton { width: Math.max(90, root.width / 6); text: "Godzinowo" }
            TabButton { width: Math.max(90, root.width / 6); text: "Dni" }
            TabButton { width: Math.max(90, root.width / 6); text: "Mapy" }
            TabButton { width: Math.max(90, root.width / 6); text: "Synoptyk" }
            TabButton { width: Math.max(90, root.width / 6); text: "Artykuły" }
        }
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: root.viewMode === 3 ? mapsComponent : root.viewMode >= 4 ? contentComponent : forecastComponent
        }
    }
    Component {
        id: forecastComponent
        ColumnLayout {
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Kirigami.Icon { source: root.weather.weatherIcon; Layout.preferredWidth: 44; Layout.preferredHeight: 44 }
                ColumnLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading { text: root.weather.locationName; level: 2; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: root.weather.summaryTime; opacity: 0.65; font.pixelSize: 11 }
                }
                Kirigami.Heading { text: root.weather.temperatureText; level: 1 }
                ToolButton { icon.name: "mark-location"; text: "Lokalizacja"; display: AbstractButton.IconOnly; onClicked: root.configureRequested(); ToolTip.text: text; ToolTip.visible: hovered }
                ToolButton { icon.name: "configure"; text: "Wygląd meteogramu"; display: AbstractButton.IconOnly; onClicked: displayDialog.open(); ToolTip.text: text; ToolTip.visible: hovered }
            }
            Label { Layout.fillWidth: true; text: root.weather.modelName + " · " + root.weather.runText + " · " + (root.weather.utc ? "UTC" : "czas lokalny systemu"); opacity: 0.65; font.pixelSize: 11; wrapMode: Text.WordWrap }
            Label { Layout.fillWidth: true; text: root.weather.latitude.toFixed(4) + "°N, " + root.weather.longitude.toFixed(4) + "°E · " + root.weather.sunTimes; visible: root.weather.hasData; opacity: 0.65; font.pixelSize: 11; wrapMode: Text.WordWrap }
            Kirigami.InlineMessage { visible: root.weather.error.length > 0; text: root.weather.error; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: root.weather.archive; text: "Archiwalne uruchomienie modelu. Daty i godziny wykresu odnoszą się do wybranej prognozy."; type: Kirigami.MessageType.Information; Layout.fillWidth: true }
            Kirigami.InlineMessage { visible: root.weather.stale; text: "Prognoza może być nieaktualna. Ostatnie pobranie: " + root.weather.updatedText; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
            GridLayout {
                visible: root.viewMode === 0
                Layout.fillWidth: true
                columns: width < 600 ? 2 : 4
                ComboBox {
                    Layout.fillWidth: true
                    model: UserPreferences.profiles
                    textRole: "name"
                    currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
                    onActivated: index => UserPreferences.profile = model[index].id
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["Cała prognoza", "24 godziny", "48 godzin", "72 godziny"]
                    onActivated: index => root.setHours([0,24,48,72][index])
                }
                ComboBox {
                    id: archiveCombo
                    Layout.fillWidth: true
                    model: root.weather.availableRuns.length ? root.weather.availableRuns : [{timestamp: 0, label: "Najnowsza · archiwum ↻"}]
                    textRole: "label"
                    onDownChanged: if (down) root.weather.loadRuns()
                    currentIndex: { const entries = root.weather.availableRuns; const index = entries.findIndex(e => e.timestamp === root.weather.requestedRun); return Math.max(0, index); }
                    onActivated: index => {
                        if (!root.weather.availableRuns.length) root.weather.refresh();
                        else root.weather.requestedRun = root.weather.availableRuns[index].timestamp;
                    }
                }
                RowLayout {
                    Button { Layout.preferredWidth: 32; text: "‹"; ToolTip.text: "Wcześniejsze godziny"; ToolTip.visible: hovered; onClicked: root.moveRange(-1) }
                    Button { Layout.preferredWidth: 32; text: "−"; onClicked: root.zoom(1.5, (root.viewportStart + root.viewportEnd)/2) }
                    Button { Layout.preferredWidth: 32; text: "+"; onClicked: root.zoom(0.67, (root.viewportStart + root.viewportEnd)/2) }
                    Button { Layout.preferredWidth: 32; text: "›"; ToolTip.text: "Późniejsze godziny"; ToolTip.visible: hovered; onClicked: root.moveRange(1) }
                }
            }
            Slider {
                Layout.fillWidth: true
                visible: root.viewMode === 0 && root.viewportEnd - root.viewportStart < root.weather.lastTimestamp - root.weather.firstTimestamp - 1
                from: root.weather.firstTimestamp
                to: Math.max(from, root.weather.lastTimestamp - (root.viewportEnd - root.viewportStart))
                value: root.viewportStart
                onMoved: { const span = root.viewportEnd - root.viewportStart; root.viewportStart = value; root.viewportEnd = value + span; }
            }
            Button {
                visible: root.viewMode === 0 && root.weather.model !== "gfs"
                text: root.originalView ? "Wróć do wykresów interaktywnych" : "Pokaż oryginalny meteogram ICM"
                icon.name: root.originalView ? "view-statistics" : "image-x-generic"
                onClicked: root.originalView = !root.originalView
            }
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
                sourceComponent: root.viewMode === 0 ? (root.originalView && root.weather.model !== "gfs" ? originalComponent : chartsComponent) : root.viewMode === 1 ? hourlyComponent : dailyComponent
            }
            Label {
                Layout.fillWidth: true
                text: "<a href='https://www.meteo.pl/'>ICM UW / meteo.pl</a> · UM: Met Office · " + (root.weather.usingCache ? "Z pamięci " : "Pobrano ") + root.weather.updatedText
                textFormat: Text.RichText
                font.pixelSize: 10
                opacity: 0.7
                wrapMode: Text.WordWrap
                onLinkActivated: link => Qt.openUrlExternally(link)
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
                    model: root.weather.hasData ? root.visibleCharts : []
                    delegate: Meteogram {
                        required property var modelData
                        Layout.fillWidth: true
                        weather: root.weather
                        title: modelData.title
                        groupKey: modelData.key
                        fields: modelData.fields
                        startTime: root.viewportStart
                        endTime: root.viewportEnd
                        selectedTime: root.selectedTime
                        nights: UserPreferences.showNight ? root.weather.nightIntervals : []
                        onProbeMoved: (timestamp, position) => root.showProbe(timestamp, position, groupKey)
                        onProbeLeft: closeProbe.restart()
                        onPinRequested: root.pinned = !root.pinned
                        onZoomRequested: (factor, center) => root.zoom(factor, center)
                    }
                }
            }
        }
    }
    Component { id: originalComponent; OriginalMeteogram { weather: root.weather } }
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
                        onClicked: { root.pinned = false; root.showProbe(modelData.time, mapToItem(null, width/2, 0), ""); root.pinned = true; }
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
            contentWidth: availableWidth
            clip: true
            GridLayout {
                width: dailyScroll.availableWidth
                columns: Math.max(1, Math.floor(width / 220))
                Repeater {
                    model: root.weather.dailyForecast
                    delegate: Frame {
                        id: dayCard
                        required property var modelData
                        Layout.fillWidth: true
                        padding: 16
                        ColumnLayout {
                            anchors.fill: parent
                            Label { text: dayCard.modelData.day; font.bold: true }
                            Kirigami.Heading { text: root.number(dayCard.modelData.maximum, 0) + "° / " + root.number(dayCard.modelData.minimum, 0) + "°"; level: 2 }
                            Label { text: "Opad: " + root.number(dayCard.modelData.rain, 1) + " mm" }
                            Label { text: "Porywy: " + root.number(dayCard.modelData.wind, 0) + " " + root.weather.windUnit }
                        }
                    }
                }
            }
        }
    }
    Component { id: mapsComponent; WeatherMap { weather: root.weather; onLocationSelected: (name, latitude, longitude) => { root.locationSelected(name, latitude, longitude); root.viewMode = 0; } } }
    Component { id: contentComponent; EditorialView { commentary: root.viewMode === 4 } }
    Timer { id: closeProbe; interval: 220; onTriggered: if (!root.pinned && !inspectorHover.hovered) inspector.close() }
    Popup {
        id: inspector
        objectName: "forecastInspector"
        parent: Overlay.overlay
        width: Math.min(370, parent ? parent.width - 24 : 370)
        padding: 14
        modal: false
        focus: true
        dim: false
        closePolicy: Popup.CloseOnEscape
        onHeightChanged: if (parent && opened) y = Math.max(8, Math.min(y, parent.height - height - 8))
        onClosed: { root.selectedTime = 0; root.pinned = false; }
        HoverHandler { id: inspectorHover; onHoveredChanged: if (!hovered) closeProbe.restart() }
        contentItem: ColumnLayout {
            RowLayout {
                Label { text: root.weather.formatTime(root.selectedTime, "ddd d MMM · HH:mm t"); font.bold: true; Layout.fillWidth: true }
                ToolButton { icon.name: root.pinned ? "pin" : "window-pin"; checkable: true; checked: root.pinned; onClicked: root.pinned = checked; ToolTip.text: "Przypnij odczyt"; ToolTip.visible: hovered }
                ToolButton { icon.name: "dialog-close"; onClicked: inspector.close() }
            }
            Label { text: root.pinned ? "Odczyt przypięty · kliknij ponownie wykres, aby odpiąć" : "Kliknij wykres, aby przypiąć odczyt"; opacity: 0.6; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView {
                id: inspectorScroll
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(readings.implicitHeight, inspector.parent ? inspector.parent.height - 150 : 440, 530)
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    id: readings
                    width: inspectorScroll.availableWidth
                    spacing: 2
                    Repeater {
                        model: root.allDetails
                        delegate: Rectangle {
                            id: reading
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: Math.max(nameLabel.implicitHeight, valueLabel.implicitHeight) + 12
                            color: modelData.group === root.selectedGroup ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                Rectangle { Layout.preferredWidth: 4; Layout.preferredHeight: 15; color: Kirigami.Theme.backgroundColor.hslLightness < 0.5 ? reading.modelData.darkColor : reading.modelData.color }
                                Label { id: nameLabel; text: reading.modelData.label; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 11 }
                                Label { id: valueLabel; text: reading.modelData.value; font.bold: true; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: displayDialog
        objectName: "chartSettingsDialog"
        title: "Wygląd meteogramu"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(650, parent ? parent.width - 32 : 650)
        height: Math.min(740, parent ? parent.height - 32 : 740)
        padding: 20
        modal: true
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            id: settingsScroll
            contentWidth: availableWidth
            clip: true
            DisplaySettings { width: settingsScroll.availableWidth }
        }
    }
}
