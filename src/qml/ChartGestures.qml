pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    required property real startTime
    required property real endTime
    required property real minimumSpan
    required property real maximumSpan
    readonly property bool active: pan.active || pinch.active
    signal rangeRequested(real first, real last)
    signal resetRequested()

    DragHandler {
        id: pan
        target: null
        acceptedDevices: PointerDevice.TouchScreen
        maximumPointCount: 1
        yAxis.enabled: false
        property real initialStart: 0
        property real initialSpan: 0
        onActiveChanged: if (active) {
            initialStart = root.startTime;
            initialSpan = root.endTime - root.startTime;
        }
        onActiveTranslationChanged: if (active) {
            const first = initialStart - activeTranslation.x / Math.max(1, root.width) * initialSpan;
            root.rangeRequested(first, first + initialSpan);
        }
    }
    PinchHandler {
        id: pinch
        target: null
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.TouchPad
        rotationAxis.enabled: false
        yAxis.enabled: false
        property real initialSpan: 0
        property real anchorTime: 0
        onActiveChanged: if (active) {
            initialSpan = root.endTime - root.startTime;
            anchorTime = root.startTime + centroid.position.x / Math.max(1, root.width) * initialSpan;
        }
        function updateRange() {
            if (!active || initialSpan <= 0) return;
            const span = Math.min(root.maximumSpan, Math.max(root.minimumSpan, initialSpan / activeScale));
            const first = anchorTime - centroid.position.x / Math.max(1, root.width) * span;
            root.rangeRequested(first, first + span);
        }
        onActiveScaleChanged: updateRange()
        onActiveTranslationChanged: updateRange()
    }
    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen
        onDoubleTapped: root.resetRequested()
    }
}
