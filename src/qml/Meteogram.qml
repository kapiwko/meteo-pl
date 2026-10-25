import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property string title
    required property string unit
    required property var points
    property var secondaryPoints: []
    property string secondaryTitle: ""
    property color lineColor: Kirigami.Theme.highlightColor
    property color secondaryColor: Kirigami.Theme.neutralTextColor
    property bool bars: false
    property real startTime: 0
    property real endTime: 1
    property real minimum: NaN
    property real maximum: NaN
    property int hoverIndex: -1
    property real nowTime: Date.now() / 1000
    readonly property real plotLeft: 44
    readonly property real plotRight: 12
    readonly property real plotTop: 12
    readonly property real plotBottom: 27
    readonly property color gridColor: Kirigami.Theme.disabledTextColor
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property var bounds: calculateBounds()
    spacing: 3

    function valid(value) { return value !== null && value !== undefined && isFinite(value); }
    function calculateBounds() {
        let low = Infinity;
        let high = -Infinity;
        for (const list of [points, secondaryPoints]) {
            for (const point of list) {
                if (!valid(point.value)) continue;
                low = Math.min(low, point.value);
                high = Math.max(high, point.value);
            }
        }
        if (!isFinite(low)) return { low: 0, high: 1 };
        if (bars) low = Math.min(0, low);
        const padding = Math.max((high - low) * 0.12, 1);
        return { low: isNaN(minimum) ? (bars ? low : low - padding) : minimum,
                 high: isNaN(maximum) ? high + padding : maximum };
    }
    function xForTime(time) { return plotLeft + (time - startTime) / Math.max(1, endTime - startTime) * (plot.width - plotLeft - plotRight); }
    function yForValue(value) { return plotTop + (bounds.high - value) / Math.max(0.01, bounds.high - bounds.low) * (plot.height - plotTop - plotBottom); }
    function hoverText() {
        if (hoverIndex < 0 || hoverIndex >= points.length) return "";
        const point = points[hoverIndex];
        const value = valid(point.value) ? Number(point.value).toLocaleString(Qt.locale(), 'f', 1) + " " + unit : "brak danych";
        return Qt.formatDateTime(new Date(point.time * 1000), "ddd d MMM, HH:mm") + " · " + value;
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: root.title; font.bold: true }
        Label { text: root.unit; opacity: 0.65; font.pixelSize: 11 }
        Item { Layout.fillWidth: true }
        Label { visible: root.secondaryTitle.length > 0; text: root.secondaryTitle; color: root.secondaryColor; font.pixelSize: 11 }
    }
    Canvas {
        id: plot
        Layout.fillWidth: true
        Layout.preferredHeight: 130
        renderTarget: Canvas.Image
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const left = root.plotLeft, right = width - root.plotRight;
            const top = root.plotTop, bottom = height - root.plotBottom;
            if (right <= left || bottom <= top) return;
            ctx.font = "10px sans-serif";
            ctx.textBaseline = "middle";
            for (let i = 0; i <= 3; ++i) {
                const value = root.bounds.low + (root.bounds.high - root.bounds.low) * i / 3;
                const py = root.yForValue(value);
                ctx.strokeStyle = root.gridColor;
                ctx.globalAlpha = 0.2;
                ctx.beginPath(); ctx.moveTo(left, py); ctx.lineTo(right, py); ctx.stroke();
                ctx.globalAlpha = 0.7;
                ctx.fillStyle = root.textColor;
                ctx.textAlign = "right";
                ctx.fillText(value.toFixed(root.bounds.high - root.bounds.low < 5 ? 1 : 0), left - 7, py);
            }
            const hours = (root.endTime - root.startTime) / 3600;
            const step = hours > 150 ? 48 : hours > 72 ? 24 : 12;
            ctx.textAlign = "center";
            for (let time = root.startTime; time <= root.endTime; time += step * 3600) {
                const px = root.xForTime(time);
                ctx.fillText(Qt.formatDateTime(new Date(time * 1000), "ddd HH:mm"), Math.max(left + 27, Math.min(right - 27, px)), bottom + 17);
            }
            function drawLine(list, color, isBars) {
                ctx.strokeStyle = color;
                ctx.fillStyle = color;
                ctx.lineWidth = 2;
                ctx.globalAlpha = 1;
                ctx.beginPath();
                let started = false;
                const barWidth = Math.max(2, (right - left) / Math.max(1, list.length) * 0.7);
                for (const point of list) {
                    if (!root.valid(point.value)) { started = false; continue; }
                    const px = root.xForTime(point.time), py = root.yForValue(point.value);
                    if (isBars) ctx.fillRect(px - barWidth / 2, py, barWidth, Math.max(0, root.yForValue(0) - py));
                    else if (!started) { ctx.moveTo(px, py); started = true; }
                    else ctx.lineTo(px, py);
                }
                if (!isBars) ctx.stroke();
            }
            ctx.save();
            ctx.beginPath(); ctx.rect(left, top, right - left, bottom - top); ctx.clip();
            drawLine(root.secondaryPoints, root.secondaryColor, false);
            drawLine(root.points, root.lineColor, root.bars);
            if (root.nowTime >= root.startTime && root.nowTime <= root.endTime) {
                ctx.strokeStyle = root.textColor;
                ctx.globalAlpha = 0.5;
                ctx.setLineDash([3, 4]);
                ctx.beginPath(); ctx.moveTo(root.xForTime(root.nowTime), top); ctx.lineTo(root.xForTime(root.nowTime), bottom); ctx.stroke();
                ctx.setLineDash([]);
            }
            if (root.hoverIndex >= 0 && root.hoverIndex < root.points.length) {
                ctx.globalAlpha = 0.6;
                ctx.strokeStyle = root.textColor;
                const px = root.xForTime(root.points[root.hoverIndex].time);
                ctx.beginPath(); ctx.moveTo(px, top); ctx.lineTo(px, bottom); ctx.stroke();
            }
            ctx.restore();
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onPositionChanged: mouse => {
                if (!root.points.length) return;
                const time = root.startTime + (mouse.x - root.plotLeft) / Math.max(1, width - root.plotLeft - root.plotRight) * (root.endTime - root.startTime);
                let closest = 0;
                for (let i = 1; i < root.points.length; ++i)
                    if (Math.abs(root.points[i].time - time) < Math.abs(root.points[closest].time - time)) closest = i;
                root.hoverIndex = closest;
            }
            onExited: root.hoverIndex = -1
        }
        ToolTip.visible: root.hoverIndex >= 0
        ToolTip.text: root.hoverText()
    }
    Connections {
        target: root
        function onPointsChanged() { plot.requestPaint(); }
        function onSecondaryPointsChanged() { plot.requestPaint(); }
        function onBoundsChanged() { plot.requestPaint(); }
        function onHoverIndexChanged() { plot.requestPaint(); }
        function onStartTimeChanged() { plot.requestPaint(); }
        function onEndTimeChanged() { plot.requestPaint(); }
        function onTextColorChanged() { plot.requestPaint(); }
        function onGridColorChanged() { plot.requestPaint(); }
        function onLineColorChanged() { plot.requestPaint(); }
        function onSecondaryColorChanged() { plot.requestPaint(); }
        function onNowTimeChanged() { plot.requestPaint(); }
    }
    Timer { interval: 60000; repeat: true; running: true; onTriggered: root.nowTime = Date.now() / 1000 }
}
