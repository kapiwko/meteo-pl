pragma ComponentBehavior: Bound
import QtQuick

// Both Images have immutable sources. The visible one survives loading errors,
// rapid scrubbing and cancellation of an obsolete request.
Item {
    id: root
    property url source
    property string label: ""
    property var front: null
    property var pending: null
    property string error: ""
    readonly property bool loading: requestDelay.running || pending !== null
    readonly property url displayedSource: front ? front.originalSource : ""
    readonly property string displayedLabel: front ? front.label : ""
    implicitWidth: front ? front.sourceSize.width : 0
    implicitHeight: front ? front.sourceSize.height : 0
    signal frameCommitted()
    signal failed()
    onSourceChanged: requestDelay.restart()
    function load() {
        if (pending) { pending.destroy(); pending = null; }
        error = "";
        if (!source.toString().length || source.toString() === displayedSource.toString()) return;
        pending = picture.createObject(root, {originalSource: source, label: label});
    }
    function complete(image) {
        if (pending !== image) return;
        if (image.status === Image.Ready) {
            const previous = front;
            front = image;
            pending = null;
            front.visible = true;
            if (previous) previous.destroy();
            frameCommitted();
        } else if (image.status === Image.Error) {
            pending = null;
            image.destroy();
            error = I18n.i18n("Could not download the map. Keeping the previous image visible.");
            failed();
        }
    }
    Timer { id: requestDelay; interval: 40; onTriggered: root.load() }
    Component {
        id: picture
        Image {
            id: image
            required property url originalSource
            required property string label
            anchors.fill: parent
            visible: false
            asynchronous: true
            cache: false // bounded, shared RAM/disk cache is provided by MapImages
            source: MapImages.cached(originalSource)
            fillMode: Image.PreserveAspectFit
            onStatusChanged: Qt.callLater(() => root.complete(image))
        }
    }
}
