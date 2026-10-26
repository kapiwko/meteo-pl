pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami

// Two stable image items: the current frame and the next frame. Preparing the
// next one below the current frame lets the scene graph upload its texture.
Item {
    id: root
    property url source
    property string label: ""
    readonly property color backgroundColor: Kirigami.Theme.backgroundColor
    property var front: null
    property var pending: null
    property string error: ""
    readonly property bool loading: requestDelay.running || pending !== null && !pending.preparing
    readonly property url displayedSource: front ? front.originalSource : ""
    readonly property string displayedLabel: front ? front.label : ""
    implicitWidth: front ? front.sourceSize.width : 0
    implicitHeight: front ? front.sourceSize.height : 0
    signal frameCommitted()
    signal failed()
    onSourceChanged: {
        if (source.toString() === displayedSource.toString()) return;
        if (pending && pending.originalSource.toString() === source.toString()) {
            requestDelay.stop(); pending.preparing = false; complete(pending);
        } else {
            cancelPending(); requestDelay.restart();
        }
    }
    function cancelPending() {
        const old = pending; pending = null;
        if (old) { old.visible = false; old.source = ""; }
    }
    function reload() { requestDelay.restart(); }
    function load() {
        if (!source.toString().length || source.toString() === displayedSource.toString()) return;
        request(source, label, false);
    }
    function prepare(url, nextLabel) {
        if (!front || loading || !url.toString().length || url.toString() === displayedSource.toString()) return;
        if (pending && pending.originalSource.toString() === url.toString()) return;
        request(url, nextLabel, true);
    }
    function request(url, nextLabel, preparing) {
        cancelPending(); error = "";
        const image = front === first ? second : first;
        pending = image;
        image.preparing = preparing;
        image.originalSource = url;
        image.label = nextLabel;
        image.source = MapImages.cached(url);
        Qt.callLater(() => root.complete(image));
    }
    function complete(image) {
        if (pending !== image) return;
        if (image.status === Image.Ready) {
            image.visible = true;
            if (image.preparing) { image.z = 0; return; }
            if (image.originalSource.toString() !== source.toString()) return;
            const previous = front;
            front = image; pending = null;
            image.z = 1;
            if (previous) previous.visible = false;
            frameCommitted();
        } else if (image.status === Image.Error) {
            const preparing = image.preparing;
            cancelPending();
            if (preparing) return;
            error = I18n.i18n("Could not download the map. Keeping the previous image visible.");
            failed();
        }
    }
    Timer { id: requestDelay; interval: 40; onTriggered: root.load() }
    component Buffer: Image {
        id: image
        property url originalSource
        property string label: ""
        property bool preparing: false
        anchors.fill: parent
        visible: false
        asynchronous: true
        cache: false
        fillMode: Image.PreserveAspectFit
        onStatusChanged: Qt.callLater(() => root.complete(image))
    }
    // Opaque paper prevents a prepared future frame showing through transparent
    // regions of the displayed PNG, while its image remains in the scene.
    Rectangle { anchors.fill: parent; z: 0.5; visible: root.front !== null; color: root.backgroundColor }
    Buffer { id: first }
    Buffer { id: second }
}
