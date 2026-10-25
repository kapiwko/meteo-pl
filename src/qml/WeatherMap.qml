pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property var picked: null
    property bool thematic: false
    readonly property string animationKey: [thematic, weather.model, layers.layerIndex, layers.runIndex, JSON.stringify(layers.runs), map.zoom, map.firstX, map.firstY, map.columns, map.rows].join(":")
    onThematicChanged: play.checked = false
    onAnimationKeyChanged: { animationCache.cancel(); if (play.checked) prepareDelay.restart(); }
    function prepareAnimation() {
        if (play.checked && !map.loading && map.hasFrame && !layers.busy && !animationCache.busy && !animationCache.ready)
            animationCache.start(map.animationUrls(layers.firstStep, layers.lastStep, weather.model === "gfs" ? 3 : 1));
    }
    ImagePreloader {
        id: animationCache
        onFinished: map.prefetchStep(root.nextStep())
        onFailed: play.checked = false
    }
    Timer { id: prepareDelay; interval: 120; onTriggered: root.prepareAnimation() }
    function nextStep() { return layers.step >= layers.lastStep ? layers.firstStep : layers.step + (weather.model === "gfs" ? 3 : 1); }
    signal locationSelected(string name, real latitude, real longitude)
    RowLayout {
        Layout.fillWidth: true
        Button { text: "Mapa interaktywna"; checkable: true; checked: !root.thematic; onClicked: root.thematic = false }
        Button { text: "Mapy tematyczne UM / WAM"; checkable: true; checked: root.thematic; onClicked: root.thematic = true }
    }
    Loader { Layout.fillWidth: true; Layout.fillHeight: true; active: root.thematic; visible: active; sourceComponent: ThematicMaps { weather: root.weather } }
    ColumnLayout {
    visible: !root.thematic
    Layout.fillWidth: true
    Layout.fillHeight: true
    WeatherLayersModel { id: layers; model: root.weather.model; active: !root.thematic }
    GridLayout {
        Layout.fillWidth: true
        columns: width < 550 ? 1 : 2
        ComboBox { Layout.fillWidth: true; model: layers.layers; textRole: "label"; currentIndex: layers.layerIndex; onActivated: index => layers.layerIndex = index }
        ComboBox { Layout.fillWidth: true; model: layers.runs; textRole: "label"; currentIndex: layers.runIndex; onActivated: index => layers.runIndex = index }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { id: play; text: checked ? animationCache.ready ? "Pauza" : "Anuluj ładowanie" : "Animuj"; icon.name: checked ? "media-playback-pause" : "media-playback-start"; checkable: true; enabled: layers.runs.length > 0; onCheckedChanged: { if (checked) root.prepareAnimation(); else animationCache.cancel(); } }
        Slider { Layout.fillWidth: true; from: layers.firstStep; to: layers.lastStep; stepSize: root.weather.model === "gfs" ? 3 : 1; value: layers.step; onMoved: layers.step = Math.round(value) }
        Label { text: "+" + layers.step + " h" }
        ToolButton { icon.name: "view-refresh"; onClicked: layers.reload(); enabled: !layers.busy }
    }
    Label { text: map.displayedTimeLabel || layers.timeLabel; font.bold: true }
    Kirigami.InlineMessage { text: layers.error; visible: text.length > 0; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
    Kirigami.InlineMessage { text: animationCache.error; visible: text.length > 0; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
    Kirigami.InlineMessage {
        text: map.error; visible: text.length > 0; type: Kirigami.MessageType.Warning; Layout.fillWidth: true
        actions: Kirigami.Action { text: "Ponów"; onTriggered: map.scheduleFrame() }
    }
    TileMap {
        id: map
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 260
        latitude: root.weather.latitude
        longitude: root.weather.longitude
        markerLatitude: root.picked ? root.picked.latitude : root.weather.latitude
        markerLongitude: root.picked ? root.picked.longitude : root.weather.longitude
        markerName: root.picked ? "Punkt na mapie" : root.weather.locationName
        places: UserPreferences.places
        zoom: 6
        weatherLayer: layers
        active: !root.thematic
        onPlaceSelected: (name, latitude, longitude) => { root.picked = null; root.locationSelected(name, latitude, longitude); }
        onCoordinateSelected: (latitude, longitude) => root.picked = {latitude: latitude, longitude: longitude}
        onFrameCommitted: if (play.checked) { root.prepareAnimation(); if (animationCache.ready) prefetchStep(root.nextStep()); }
        onErrorChanged: if (error.length) play.checked = false
        Column {
            z: 3
            anchors.centerIn: parent
            spacing: 8
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: layers.busy || map.loading || animationCache.busy; visible: running }
            Label { visible: animationCache.busy; text: "Przygotowywanie animacji: " + animationCache.completed + " / " + animationCache.total; padding: 8; background: Rectangle { radius: 4; color: Kirigami.Theme.backgroundColor } }
            ProgressBar { anchors.horizontalCenter: parent.horizontalCenter; width: 200; visible: animationCache.busy; from: 0; to: Math.max(1, animationCache.total); value: animationCache.completed }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button {
            visible: root.picked !== null
            text: "Meteogram dla wskazanego punktu"
            icon.name: "view-statistics"
            onClicked: root.locationSelected("Punkt na mapie", Number(root.picked.latitude.toFixed(6)), Number(root.picked.longitude.toFixed(6)))
        }
        Label { visible: root.picked !== null; text: root.picked ? root.picked.latitude.toFixed(3) + "°N, " + root.picked.longitude.toFixed(3) + "°E" : ""; opacity: 0.7 }
    }
    RowLayout {
        Layout.fillWidth: true
        ToolButton { id: legendToggle; text: "Legenda"; checkable: true }
        Label { text: "Prognoza modelu " + (root.weather.model === "gfs" ? "GFS" : "UM4") + " · dane ICM UW"; opacity: 0.65; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    ScrollView {
        visible: legendToggle.checked
        Layout.fillWidth: true
        Layout.preferredHeight: 130
        Image { source: legendToggle.checked ? MapImages.cached(layers.legendUrl) : ""; cache: false; asynchronous: true; fillMode: Image.PreserveAspectFit }
    }
    Timer { interval: 1300; repeat: true; running: play.checked && animationCache.ready && !root.thematic && !layers.busy && !map.loading && map.hasFrame && !map.error.length; onTriggered: layers.step = root.nextStep() }
    }
}
