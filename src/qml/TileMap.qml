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
    property string markerName: ""
    property var places: []
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
    signal placeSelected(string name, real latitude, real longitude)
    signal frameCommitted()
    readonly property real worldSize: 256 * Math.pow(2, zoom)
    readonly property real centerX: (longitude + 180) / 360 * worldSize
    readonly property real centerY: projectY(latitude)
    readonly property int columns: Math.max(1, Math.ceil((centerX + width / 2) / 256) - firstX)
    readonly property int rows: Math.max(1, Math.ceil((centerY + height / 2) / 256) - firstY)
    readonly property int firstX: Math.floor((centerX - width / 2) / 256)
    readonly property int firstY: Math.floor((centerY - height / 2) / 256)
    readonly property var namedPlaces: {
        const result = places.filter(place => place.name && Number.isFinite(Number(place.latitude)) && Number.isFinite(Number(place.longitude)));
        if (markerName.length && !result.some(place => sameCoordinates(place.latitude, place.longitude, markerLatitude, markerLongitude)))
            result.push({name: markerName, latitude: markerLatitude, longitude: markerLongitude});
        return result;
    }
    function sameCoordinates(lat1, lon1, lat2, lon2) { return LocationRules.samePlace(lat1, lon1, lat2, lon2); }
    function positionFor(lat, lon) {
        const rawX = (lon + 180) / 360 * worldSize - centerX;
        const dx = ((rawX + worldSize/2) % worldSize + worldSize) % worldSize - worldSize/2;
        return Qt.point(width/2 + dx, height/2 + projectY(lat) - centerY);
    }
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
                const images = [{url: tileAddress(baseTileTemplate, wrapped, y, zoom), opacity: 1, shared: true}];
                if (weatherLayer) {
                    const url = weatherLayer.tileUrl(wrapped, y, zoom).toString();
                    if (!url.length) return; // model's published runs are still loading
                    images.push({url: url, opacity: 0.85});
                    if (labelTileTemplate.length) images.push({url: tileAddress(labelTileTemplate, wrapped, y, zoom), opacity: 1, shared: true});
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
        error = I18n.i18n("Could not download all tiles. Keeping the previous map visible.");
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
        const steps = Math.floor((lastStep - firstStep) / stride) + 1;
        const current = Math.max(0, Math.min(steps - 1, Math.floor((weatherLayer.step - firstStep) / stride)));
        for (let index = 0; index < steps; ++index) {
            const step = firstStep + ((current + index) % steps) * stride;
            for (const tile of front.tiles) {
                const x = ((tile.x % count) + count) % count;
                urls.push(weatherLayer.tileUrlAtStep(x, tile.y, front.level, step).toString());
            }
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
    Item {
        anchors.fill: parent
        TapHandler {
            onTapped: eventPoint => {
                const coordinate = root.atPoint(eventPoint.position.x, eventPoint.position.y);
                root.coordinateSelected(coordinate.latitude, coordinate.longitude);
            }
        }
    }
    Rectangle {
        readonly property point position: root.positionFor(root.markerLatitude, root.markerLongitude)
        x: position.x - width/2
        y: position.y - height/2
        visible: !root.namedPlaces.some(place => root.sameCoordinates(place.latitude, place.longitude, root.markerLatitude, root.markerLongitude))
        width: 18; height: 18; radius: 9
        color: Kirigami.Theme.highlightColor
        border.color: "white"
        border.width: 3
    }
    Item {
        anchors.fill: parent
        z: 1
        Repeater {
            model: root.namedPlaces
            delegate: Item {
                id: placeMarker
                required property var modelData
                required property int index
                readonly property point position: root.positionFor(Number(modelData.latitude), Number(modelData.longitude))
                readonly property bool current: root.sameCoordinates(modelData.latitude, modelData.longitude, root.markerLatitude, root.markerLongitude)
                anchors.fill: parent
                z: current ? 1 : 0
                visible: position.x >= 0 && position.x <= root.width && position.y >= 0 && position.y <= root.height
                Rectangle {
                    x: placeMarker.position.x - width/2
                    y: placeMarker.position.y - height/2
                    width: 12; height: 12; radius: 6
                    color: placeMarker.current ? Kirigami.Theme.highlightColor : Kirigami.Theme.neutralTextColor
                    border.color: Kirigami.Theme.backgroundColor
                    border.width: 2
                }
                Button {
                    objectName: "map-place-label-" + placeMarker.index
                    readonly property point coordinatePosition: placeMarker.position
                    text: placeMarker.modelData.name
                    width: Math.min(implicitWidth, 180, Math.max(1, root.width - 16))
                    x: Math.max(4, Math.min(root.width - width - 4, placeMarker.position.x + width + 14 <= root.width ? placeMarker.position.x + 10 : placeMarker.position.x - width - 10))
                    y: Math.max(4, Math.min(root.height - height - 4, placeMarker.position.y - height/2))
                    highlighted: placeMarker.current
                    font.bold: placeMarker.current
                    onClicked: root.placeSelected(placeMarker.modelData.name, Number(placeMarker.modelData.latitude), Number(placeMarker.modelData.longitude))
                    ToolTip.text: I18n.i18n("Show forecast: ") + text
                    ToolTip.visible: hovered
                    Accessible.name: ToolTip.text
                    Accessible.description: placeMarker.current ? I18n.i18n("Selected location") : ""
                }
            }
        }
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
    MapWheelHandler { onZoomRequested: (steps, position) => root.zoomAt(steps, position.x, position.y) }
    Column {
        z: 2
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: Kirigami.Units.smallSpacing
        RoundButton { objectName: "mapZoomIn"; icon.name: "zoom-in"; width: 48; height: 48; Accessible.name: I18n.i18n("Zoom in"); onClicked: root.zoomAt(1, root.width / 2, root.height / 2) }
        RoundButton { objectName: "mapZoomOut"; icon.name: "zoom-out"; width: 48; height: 48; Accessible.name: I18n.i18n("Zoom out"); onClicked: root.zoomAt(-1, root.width / 2, root.height / 2) }
    }
    Rectangle {
        z: 2
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
