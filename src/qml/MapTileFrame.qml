pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: frame
    // A strong TileMap reference creates a recursive QML type dependency on Qt 6.8.
    required property var viewport
    required property var tiles
    required property int level
    required property int firstX
    required property int firstY
    required property int columns
    required property string frameKey
    required property string timeLabel
    property bool ready: false
    property bool failed: false
    readonly property real factor: Math.pow(2, viewport.zoom - level)
    readonly property real wrapOffset: Math.round((viewport.centerX - (firstX + columns / 2) * 256 * factor) / viewport.worldSize) * viewport.worldSize
    x: firstX * 256 * factor + wrapOffset - viewport.centerX + viewport.width / 2
    y: firstY * 256 * factor - viewport.centerY + viewport.height / 2
    scale: factor
    transformOrigin: Item.TopLeft
    visible: false
    function check() {
        if (ready || failed || tileRepeater.count !== tiles.length) return;
        for (let i = 0; i < tileRepeater.count; ++i) {
            const tile = tileRepeater.itemAt(i);
            if (!tile) return;
            if (tile.failed) { failed = true; return; }
            if (!tile.ready) return;
        }
        ready = true;
    }
    Timer { id: checkDelay; interval: 0; onTriggered: frame.check() }
    Repeater {
        id: tileRepeater
        model: frame.tiles
        delegate: Item {
            id: tile
            required property var modelData
            x: (modelData.x - frame.firstX) * 256
            y: (modelData.y - frame.firstY) * 256
            width: 256; height: 256
            property bool ready: false
            property bool failed: false
            function check() {
                let complete = 0;
                for (let i = 0; i < images.count; ++i) {
                    const image = images.itemAt(i) as Image;
                    if (!image) return;
                    if (image.status === Image.Error) { failed = true; return; }
                    if (image.status === Image.Ready) complete++;
                }
                ready = complete === images.count;
            }
            onReadyChanged: checkDelay.restart()
            onFailedChanged: checkDelay.restart()
            Repeater {
                id: images
                model: tile.modelData.images
                delegate: Image {
                    required property var modelData
                    anchors.fill: parent
                    asynchronous: true
                    // Base map and labels stay identical across forecast steps.
                    // Sharing their live pixmaps also shares GPU textures.
                    cache: Boolean(modelData.shared)
                    source: MapImages.cached(modelData.url)
                    opacity: modelData.opacity
                    onStatusChanged: tile.check()
                    Component.onCompleted: tile.check()
                }
            }
            Component.onCompleted: { check(); checkDelay.restart(); }
        }
    }
    Component.onCompleted: checkDelay.restart()
    onReadyChanged: if (ready) viewport.commitFrame(frame)
    onFailedChanged: if (failed) viewport.rejectFrame(frame)
}
