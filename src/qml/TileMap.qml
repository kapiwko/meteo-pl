pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Item {
    id: root
    property real latitude: 52.2297
    property real longitude: 21.0122
    property real markerLatitude: latitude
    property real markerLongitude: longitude
    property int zoom: 7
    property var weatherLayer: null
    property bool active: true
    property string baseTileTemplate: "https://mapy.meteo.pl/meteo_background/{z}/{x}/{y}.png?client=app"
    property string labelTileTemplate: "https://mapy.meteo.pl/meteo_overlay/{z}/{x}/{y}.png?client=app"
    property var front: null
    property var pending: null
    property string error: ""
    property string displayedTimeLabel: ""
    readonly property bool loading: requestDelay.running || pending !== null
    readonly property bool hasFrame: front !== null
    readonly property int layerRevision: weatherLayer ? weatherLayer.revision : 0
    signal coordinateSelected(real latitude, real longitude)
    signal frameCommitted()
    readonly property real worldSize: 256 * Math.pow(2, zoom)
    readonly property real centerX: (longitude + 180) / 360 * worldSize
    readonly property real centerY: projectY(latitude)
    readonly property int columns: Math.max(1, Math.ceil((centerX + width / 2) / 256) - firstX)
    readonly property int rows: Math.max(1, Math.ceil((centerY + height / 2) / 256) - firstY)
    readonly property int firstX: Math.floor((centerX - width / 2) / 256)
    readonly property int firstY: Math.floor((centerY - height / 2) / 256)
    property real dragX: 0
    property real dragY: 0
    clip: true
    implicitHeight: 320
    onFirstXChanged: scheduleFrame()
    onFirstYChanged: scheduleFrame()
    onZoomChanged: scheduleFrame()
    onColumnsChanged: scheduleFrame()
    onRowsChanged: scheduleFrame()
    onLayerRevisionChanged: scheduleFrame()
    onBaseTileTemplateChanged: scheduleFrame()
    onLabelTileTemplateChanged: scheduleFrame()
    onActiveChanged: {
        if (active) scheduleFrame();
        else { requestDelay.stop(); if (pending) { pending.destroy(); pending = null; } }
    }
    Component.onCompleted: scheduleFrame()

    function tileAddress(template, x, y, z) {
        return template.replace("{x}", x).replace("{y}", y).replace("{z}", z);
    }
    function scheduleFrame() { if (active) requestDelay.restart(); }
    function loadFrame() {
        if (!active || width <= 0 || height <= 0) return;
        const tiles = [];
        const count = Math.pow(2, zoom);
        for (let y = firstY; y < firstY + rows; ++y) {
            if (y < 0 || y >= count) continue;
            for (let x = firstX; x < firstX + columns; ++x) {
                const wrapped = ((x % count) + count) % count;
                const images = [{url: tileAddress(baseTileTemplate, wrapped, y, zoom), opacity: 1}];
                if (weatherLayer) {
                    const url = weatherLayer.tileUrl(wrapped, y, zoom).toString();
                    if (!url.length) return; // model's published runs are still loading
                    images.push({url: url, opacity: 0.85});
                    if (labelTileTemplate.length) images.push({url: tileAddress(labelTileTemplate, wrapped, y, zoom), opacity: 1});
                }
                tiles.push({x: x, y: y, images: images});
            }
        }
        const key = zoom + ":" + JSON.stringify(tiles);
        if (pending && pending.frameKey === key) return;
        if (pending) { pending.destroy(); pending = null; }
        error = "";
        if (front && front.frameKey === key) return;
        pending = frameComponent.createObject(frameContainer, {viewport: root, tiles: tiles, level: zoom, columns: columns,
            firstX: firstX, firstY: firstY, frameKey: key, timeLabel: weatherLayer ? weatherLayer.timeLabel : ""});
    }
    function commitFrame(frame) {
        if (pending !== frame) return;
        const previous = front;
        front = frame;
        pending = null;
        frame.visible = true;
        displayedTimeLabel = frame.timeLabel;
        if (previous) previous.destroy();
        frameCommitted();
    }
    function rejectFrame(frame) {
        if (pending !== frame) return;
        pending = null;
        frame.destroy();
        error = "Nie udało się pobrać wszystkich kafelków. Poprzednia mapa pozostaje widoczna.";
    }
    function prefetchStep(step) {
        if (!weatherLayer || !front) return;
        const count = Math.pow(2, front.level);
        for (const tile of front.tiles) {
            const x = ((tile.x % count) + count) % count;
            MapImages.prefetch(weatherLayer.tileUrlAtStep(x, tile.y, front.level, step));
        }
    }
    function animationUrls(firstStep, lastStep, stride) {
        const urls = [];
        if (!weatherLayer || !front) return urls;
        const count = Math.pow(2, front.level);
        for (let step = firstStep; step <= lastStep; step += stride)
            for (const tile of front.tiles) {
                const x = ((tile.x % count) + count) % count;
                urls.push(weatherLayer.tileUrlAtStep(x, tile.y, front.level, step).toString());
            }
        return urls;
    }
    Timer { id: requestDelay; interval: 90; onTriggered: root.loadFrame() }
    Component { id: frameComponent; MapTileFrame {} }

    function projectY(lat) {
        const radians = Math.max(-85.0511, Math.min(85.0511, lat)) * Math.PI / 180;
        return (1 - Math.log(Math.tan(radians) + 1 / Math.cos(radians)) / Math.PI) / 2 * worldSize;
    }
    function unproject(px, py) {
        return { latitude: Math.atan(Math.sinh(Math.PI * (1 - 2 * py / worldSize))) * 180 / Math.PI,
                 longitude: ((px / worldSize * 360) % 360 + 360) % 360 - 180 };
    }
    function atPoint(px, py) { return unproject(centerX + px - width / 2, centerY + py - height / 2); }
    function zoomAt(delta, px, py) {
        const nextZoom = Math.max(2, Math.min(13, zoom + delta));
        if (nextZoom === zoom) return;
        const factor = Math.pow(2, nextZoom - zoom);
        const nextX = (centerX + px - width / 2) * factor - px + width / 2;
        const nextY = (centerY + py - height / 2) * factor - py + height / 2;
        zoom = nextZoom;
        const center = unproject(nextX, nextY);
        latitude = Math.max(-85, Math.min(85, center.latitude));
        longitude = center.longitude;
    }
    Rectangle { anchors.fill: parent; color: Kirigami.Theme.alternateBackgroundColor }
    Item { id: frameContainer; anchors.fill: parent }
    Rectangle {
        x: (root.markerLongitude + 180) / 360 * root.worldSize - root.centerX + root.width / 2 - width / 2
        y: root.projectY(root.markerLatitude) - root.centerY + root.height / 2 - height / 2
        width: 18; height: 18; radius: 9
        color: Kirigami.Theme.highlightColor
        border.color: "white"
        border.width: 3
    }
    DragHandler {
        target: null
        onActiveChanged: { root.dragX = 0; root.dragY = 0; }
        onTranslationChanged: {
            const coordinate = root.unproject(root.centerX - translation.x + root.dragX, root.centerY - translation.y + root.dragY);
            root.latitude = Math.max(-85, Math.min(85, coordinate.latitude));
            root.longitude = coordinate.longitude;
            root.dragX = translation.x;
            root.dragY = translation.y;
        }
    }
    TapHandler {
        onTapped: eventPoint => {
            const coordinate = root.atPoint(eventPoint.position.x, eventPoint.position.y);
            root.coordinateSelected(coordinate.latitude, coordinate.longitude);
        }
    }
    MapWheelHandler { onZoomRequested: (steps, position) => root.zoomAt(steps, position.x, position.y) }
    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        Button { text: "+"; width: 36; onClicked: root.zoomAt(1, root.width / 2, root.height / 2) }
        Button { text: "−"; width: 36; onClicked: root.zoomAt(-1, root.width / 2, root.height / 2) }
    }
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: copyright.implicitWidth + 12
        height: copyright.implicitHeight + 6
        color: Kirigami.Theme.backgroundColor
        opacity: 0.9
        Label {
            id: copyright
            anchors.centerIn: parent
            text: "© OpenStreetMap contributors · ICM UW"
            font.pixelSize: 10
        }
    }
}
