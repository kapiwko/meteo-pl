pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property bool mobileLayout: false
    property var picked: null
    property bool thematic: false
    readonly property string animationKey: [thematic, weather.model, layers.layerIndex, layers.runIndex, JSON.stringify(layers.runs), map.zoom, map.firstX, map.firstY, map.columns, map.rows].join(":")
    onThematicChanged: play.checked = false
    onAnimationKeyChanged: { animationCache.cancel(); if (play.checked) prepareDelay.restart(); }
    function prepareAnimation() {
        if (play.checked && !map.loading && map.hasFrame && !layers.busy && !animationCache.busy && !animationCache.ready)
            animationCache.start(map.animationUrls(layers.firstStep, layers.lastStep, layers.stepHours));
    }
    ImagePreloader {
        id: animationCache
        readyCount: map.front ? map.front.tiles.length * 3 : 0
        onReadyChanged: if (ready) map.prefetchStep(root.nextStep())
        onFinished: map.prefetchStep(root.nextStep())
        onFailed: play.checked = false
    }
    MapLegendModel {
        id: mapLegend
        layer: layers.layers[layers.layerIndex].id
        active: !root.thematic && legendToggle.checked
    }
    Timer { id: prepareDelay; interval: 120; onTriggered: root.prepareAnimation() }
    function nextStep() { return layers.step >= layers.lastStep ? layers.firstStep : Math.min(layers.lastStep, layers.step + layers.stepHours); }
    signal locationSelected(string name, real latitude, real longitude)
    RowLayout {
        visible: !root.mobileLayout
        Layout.fillWidth: true
        Button { text: I18n.i18n("Interactive map"); checkable: true; checked: !root.thematic; onClicked: root.thematic = false }
        Button { text: I18n.i18n("UM / WAM thematic maps"); checkable: true; checked: root.thematic; onClicked: root.thematic = true }
    }
    ComboBox {
        objectName: "mobileMapType"
        visible: root.mobileLayout
        Layout.fillWidth: true
        Accessible.name: I18n.i18n("Map type")
        model: [I18n.i18n("Interactive map"), I18n.i18n("UM / WAM thematic maps")]
        currentIndex: root.thematic ? 1 : 0
        onActivated: index => root.thematic = index === 1
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
        Button { id: play; text: checked ? animationCache.ready ? I18n.i18n("Pause") : I18n.i18n("Cancel loading") : I18n.i18n("Animate"); icon.name: checked ? "media-playback-pause" : "media-playback-start"; checkable: true; enabled: layers.runs.length > 0; onCheckedChanged: { if (checked) root.prepareAnimation(); else animationCache.cancel(); } }
        Slider { Layout.fillWidth: true; from: layers.firstStep; to: layers.lastStep; stepSize: layers.stepHours; value: layers.step; onMoved: layers.step = Math.round(value) }
        Label { text: "+" + layers.step + " h" }
        ToolButton { icon.name: "view-refresh"; onClicked: layers.reload(); enabled: !layers.busy }
    }
    Label { text: map.displayedTimeLabel || layers.timeLabel; font.bold: true }
    Kirigami.InlineMessage { text: layers.error; visible: text.length > 0; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
    Kirigami.InlineMessage { text: animationCache.error; visible: text.length > 0; type: Kirigami.MessageType.Warning; Layout.fillWidth: true }
    Kirigami.InlineMessage {
        text: map.error; visible: text.length > 0; type: Kirigami.MessageType.Warning; Layout.fillWidth: true
        actions: Kirigami.Action { text: I18n.i18n("Retry"); onTriggered: map.scheduleFrame() }
    }
    TileMap {
        id: map
        Layout.fillWidth: true
        Layout.fillHeight: true
        objectName: "weatherMapViewport"
        Layout.minimumHeight: root.mobileLayout ? 160 : 260
        latitude: root.weather.latitude
        longitude: root.weather.longitude
        markerLatitude: root.picked ? root.picked.latitude : root.weather.latitude
        markerLongitude: root.picked ? root.picked.longitude : root.weather.longitude
        markerName: root.picked ? I18n.i18n("Point on map") : root.weather.locationName
        places: UserPreferences.places
        zoom: 6
        weatherLayer: layers
        active: !root.thematic
        ToolButton {
            id: legendToggle
            objectName: "mapLegendToggle"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Kirigami.Units.largeSpacing
            z: 4
            text: I18n.i18n("Legend")
            icon.name: "view-list-details"
            checkable: true
            checked: !root.mobileLayout
            ToolTip.text: text
            ToolTip.visible: hovered
        }
        MapLegend {
            legend: mapLegend
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.largeSpacing
            anchors.bottomMargin: 32
            z: 4
            visible: legendToggle.checked
            onCloseRequested: legendToggle.checked = false
        }
        onPlaceSelected: (name, latitude, longitude) => { root.picked = null; root.locationSelected(name, latitude, longitude); }
        onCoordinateSelected: (latitude, longitude) => root.picked = {latitude: latitude, longitude: longitude}
        onFrameCommitted: if (play.checked) { root.prepareAnimation(); if (animationCache.ready) prefetchStep(root.nextStep()); }
        onErrorChanged: if (error.length) play.checked = false
        Column {
            z: 3
            anchors.centerIn: parent
            spacing: 8
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: layers.busy || map.loading || animationCache.busy && !animationCache.ready; visible: running }
            Label { visible: animationCache.busy; text: (animationCache.ready ? I18n.i18n("Loading next frames: ") : I18n.i18n("Preparing animation: ")) + animationCache.completed + " / " + animationCache.total; padding: 8; background: Rectangle { radius: 4; color: Kirigami.Theme.backgroundColor } }
            ProgressBar { anchors.horizontalCenter: parent.horizontalCenter; width: 200; visible: animationCache.busy; from: 0; to: Math.max(1, animationCache.total); value: animationCache.completed }
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: width < 550 ? 1 : 2
        Button {
            Layout.fillWidth: true
            visible: root.picked !== null
            text: I18n.i18n("Meteogram for the selected point")
            icon.name: "view-statistics"
            onClicked: root.locationSelected(I18n.i18n("Point on map"), Number(root.picked.latitude.toFixed(6)), Number(root.picked.longitude.toFixed(6)))
        }
        Label { visible: root.picked !== null; text: root.picked ? root.picked.latitude.toFixed(3) + "°N, " + root.picked.longitude.toFixed(3) + "°E" : ""; opacity: 0.7 }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: I18n.i18n("Model forecast ") + (root.weather.model === "gfs" ? "GFS" : "UM4") + I18n.i18n(" · ICM UW data"); opacity: 0.65; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    Timer { interval: 500; repeat: true; running: play.checked && animationCache.ready && !root.thematic && !layers.busy && map.hasFrame && !map.error.length; onTriggered: if (!map.loading) layers.step = root.nextStep() }
    }
}
