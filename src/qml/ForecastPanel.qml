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
    readonly property real minimumViewportSpan: Math.min(24 * 3600, Math.max(0, weather.lastTimestamp - weather.firstTimestamp))
    signal configureRequested()
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
    readonly property var visibleCharts: {
        if (!weather.hasData || !Object.keys(weather.series).length) return [];
        return weather.configuredCharts(UserPreferences.chartLayout);
    }

    function openDisplaySettings() { displayDialog.open(); }
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
                                objectName: "viewOptionsButton"
                                text: "Widok"
                                icon.name: "configure"
                                Accessible.name: "Opcje widoku"
                                ToolTip.text: "Profil wykresów, zakres czasu i start modelu"; ToolTip.visible: hovered
                                onClicked: viewOptions.open()
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
                        chartConfiguration: modelData
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
                            color: root.selectedFieldKeys.indexOf(modelData.key) >= 0 ? Kirigami.Theme.alternateBackgroundColor : "transparent"
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
        id: viewOptions
        objectName: "viewOptionsDialog"
        title: "Opcje widoku"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(430, parent ? parent.width - 24 : 430)
        padding: 16
        modal: false
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 12
            GridLayout {
                Layout.fillWidth: true
                columns: 1
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "Profil wykresów"; font.pixelSize: 11; opacity: 0.7 }
                    ComboBox {
                        Layout.fillWidth: true
                        Accessible.name: "Profil wykresów"
                        model: UserPreferences.profiles
                        textRole: "name"
                        currentIndex: model.findIndex(profile => profile.id === UserPreferences.profile)
                        onActivated: index => UserPreferences.profile = model[index].id
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "Zakres czasu"; font.pixelSize: 11; opacity: 0.7 }
                    ComboBox {
                        Layout.fillWidth: true
                        Accessible.name: "Zakres czasu na wykresach"
                        model: ["Cała prognoza", "24 godziny", "48 godzin", "72 godziny", "Własny zakres"]
                        currentIndex: {
                            const hours = (root.viewportEnd - root.viewportStart) / 3600;
                            if (Math.abs(root.viewportStart - root.weather.firstTimestamp) < 1 && Math.abs(root.viewportEnd - root.weather.lastTimestamp) < 1) return 0;
                            const index = [24,48,72].findIndex(value => Math.abs(value - hours) < 0.01);
                            return index < 0 ? 4 : index + 1;
                        }
                        onActivated: index => { if (index < 4) root.setHours([0,24,48,72][index]); }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "Start modelu"; font.pixelSize: 11; opacity: 0.7 }
                    ComboBox {
                        id: archiveCombo
                        Layout.fillWidth: true
                        Accessible.name: "Start obliczeń modelu, także archiwalny"
                        model: root.weather.availableRuns.length ? root.weather.availableRuns : [{timestamp: 0, label: "Najnowszy dostępny"}]
                        textRole: "label"
                        onDownChanged: if (down) root.weather.loadRuns()
                        currentIndex: { const entries = root.weather.availableRuns; const index = entries.findIndex(e => e.timestamp === root.weather.requestedRun); return Math.max(0, index); }
                        onActivated: index => {
                            if (!root.weather.availableRuns.length) root.weather.refresh();
                            else root.weather.requestedRun = root.weather.availableRuns[index].timestamp;
                        }
                    }
                }
            }
            Button {
                text: "Edytuj profile i wygląd…"
                icon.name: "preferences-desktop-theme-global"
                onClicked: { viewOptions.close(); root.openDisplaySettings(); }
            }
            Label {
                Layout.fillWidth: true
                text: "Zakres czasu i start modelu dotyczą tego widoku. Wybrany profil jest zapamiętywany."
                font.pixelSize: 11
                opacity: 0.7
                wrapMode: Text.WordWrap
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
            DisplaySettings { width: settingsScroll.availableWidth; weather: root.weather }
        }
    }
}
