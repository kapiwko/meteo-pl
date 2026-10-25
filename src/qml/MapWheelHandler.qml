import QtQuick

WheelHandler {
    id: handler
    target: null
    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    property real remainder: 0
    signal zoomRequested(int steps, point position)
    onActiveChanged: if (!active) remainder = 0
    onWheel: event => {
        // High resolution touchpads may provide only pixelDelta. Accumulate
        // small deltas so a gesture does not jump a zoom level on every pixel.
        const delta = event.angleDelta.y || event.pixelDelta.y * 3;
        if (!delta) { event.accepted = false; return; }
        if (remainder * delta < 0) remainder = 0;
        remainder += delta;
        const steps = Math.trunc(remainder / 120);
        if (steps) {
            remainder -= steps * 120;
            zoomRequested(steps, Qt.point(event.x, event.y));
        }
        event.accepted = true;
    }
}
