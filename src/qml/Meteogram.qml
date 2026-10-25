pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "MeteogramStyle.js" as Style

ColumnLayout {
    id: root
    required property WeatherModel weather
    required property string title
    required property string groupKey
    required property var fields
    property var chartConfiguration: ({})
    property int legendOverride: -1
    property bool exportMode: false
    property bool identificationEnabled: false
    readonly property bool legendVisible: legendOverride < 0 ? UserPreferences.showLegend : Boolean(legendOverride)
    readonly property string scaleKind: chartConfiguration.leftScale || (groupKey === "ground" ? "temperature" : groupKey)
    readonly property string rightScale: chartConfiguration.rightScale || (scaleKind === "clouds" ? "fraction" : scaleKind === "cloudHeights" ? "visibility" : scaleKind === "precipitation" ? "humidity" : "linked")
    property real startTime: weather.firstTimestamp
    property real endTime: weather.lastTimestamp
    property real selectedTime: 0
    readonly property var probeSamples: {
        const result = {};
        if (selectedTime <= 0 || !Object.keys(weather.series).length) return result;
        for (const sample of weather.detailsAt(selectedTime)) result[sample.key] = sample;
        return result;
    }
    property var nights: []
    property var hiddenKeys: []
    property int chartHeight: chartConfiguration.height || Math.round((UserPreferences.profileChartHeight || UserPreferences.chartHeight) * (directionOnly ? 64 / 174 : 1))
    property real nowTime: Date.now() / 1000
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color gridColor: Kirigami.Theme.disabledTextColor
    readonly property color backgroundColor: Kirigami.Theme.backgroundColor
    readonly property bool darkPalette: backgroundColor.hslLightness < 0.5
    readonly property real leftMargin: 50
    readonly property real rightMargin: 44
    readonly property bool hasRightAxis: fields.some(f => isRightField(f)) || ["temperature", "pressure", "wind", "probability", "clouds"].indexOf(scaleKind) >= 0
    readonly property bool wind: scaleKind === "wind"
    readonly property bool directionOnly: fields.length > 0 && fields.every(f => f.kind === "direction" || f.kind === "hidden")
    readonly property real directionBandHeight: !directionOnly && fields.some(f => f.kind === "direction") ? 30 : 0
    readonly property real topMargin: directionOnly ? 8 : 28
    readonly property real bottomMargin: directionOnly ? 8 : 30 + directionBandHeight
    readonly property var bounds: calculateBounds()
    readonly property var rightBounds: calculateBounds(true)
    readonly property real windScale: weather.windUnit === "km/h" ? 3.6 : weather.windUnit === "kt" ? 1.943844492 : 1
    readonly property string exportRevision: JSON.stringify({
        data: weather.dataRevision, start: startTime, end: endTime, hidden: hiddenKeys,
        legend: legendVisible, title: title, height: chartHeight, nights: nights,
        colors: [String(textColor), String(gridColor), String(backgroundColor)], configuration: exportStyle(chartConfiguration),
        fields: fields.map(field => exportStyle(field))
    })
    signal probeMoved(real timestamp, point position)
    signal probeLeft()
    signal pinRequested(real timestamp, point position)
    signal zoomRequested(real factor, real center)
    spacing: 6
    objectName: "chart-" + groupKey

    function valid(value) { return value !== null && value !== undefined && isFinite(value); }
    function exportStyle(configuration) {
        const style = Object.assign({}, configuration);
        delete style.points; delete style.lowerPoints; delete style.fields;
        return style;
    }
    function shown(field) { return hiddenKeys.indexOf(field.key) < 0; }
    function seriesColor(field) { return darkPalette && field.darkColor ? field.darkColor : field.color; }
    function isRightField(field) { return field.axis !== undefined ? field.axis === "right" : field.kind === "right" || field.kind === "rightBars"; }
    function configuredAxis(axis, right) {
        const prefix = right ? "right" : "left", minimum = chartConfiguration[prefix + "Min"], maximum = chartConfiguration[prefix + "Max"];
        const low = valid(minimum) ? Number(minimum) : axis.low, high = valid(maximum) ? Number(maximum) : axis.high;
        return (valid(minimum) || valid(maximum)) && high > low ? ChartAnalysis.niceAxis(low, high) : axis;
    }
    function drawArea(ctx, field, bottom) {
        ctx.save();
        ctx.globalAlpha = field.opacity === undefined ? 1 : field.opacity;
        for (let i = 1; i < field.points.length; ++i) {
            const a = field.points[i-1], b = field.points[i];
            if (!valid(a.value) || !valid(b.value)) continue;
            const right = isRightField(field);
            ctx.fillStyle = field.fill === "pressure" ? Style.pressureColor(b.value, darkPalette)
                : field.fill === "wind" ? Style.windColor((a.value + b.value) / 2 / windScale, darkPalette) : seriesColor(field);
            ctx.beginPath(); ctx.moveTo(px(a.time), bottom); ctx.lineTo(px(a.time), py(a.value, right));
            ctx.lineTo(px(b.time), py(b.value, right)); ctx.lineTo(px(b.time), bottom); ctx.closePath(); ctx.fill();
        }
        ctx.restore();
    }
    function layerOrder(field) {
        if (field.layer !== undefined) return field.layer;
        // Same painter order as the ICM meteogram; independent of legend and
        // profile order. Sparse cloud bases remain above denser cover levels.
        const cover = ["cldhigh_aver", "cldmed_aver", "cldlow_aver", "cldvlow_aver"].indexOf(field.key);
        if (field.kind === "cloudArea") return cover < 0 ? 0 : cover;
        if (field.kind === "barsBack") return 0;
        const bases = ["cldbse79", "cldbse65", "cldbse45", "cldbse25", "cldbse01"];
        const base = bases.indexOf(field.key);
        if (base >= 0) return base + 1;
        if (field.key === "cldtop") return 6;
        if (field.kind === "bars") return 10;
        if (field.kind === "stems") return 11;
        if (field.key === "wchill_point") return 12;
        if (field.key === "airtmp_point") return 13;
        if (field.key === "dwptmp_point") return 14;
        if (field.kind === "gusts") return 16;
        return field.kind === "rightBars" ? 20 : 15;
    }
    function drawGrid(ctx, left, right, top, bottom, first, interval) {
        ctx.save();
        ctx.strokeStyle = darkPalette ? "#ffffff" : "#000000";
        ctx.globalAlpha = darkPalette ? 0.24 : 0.22;
        ctx.lineWidth = 0.75;
        ctx.setLineDash([2, 2]);
        if (!directionOnly) for (const value of ticks(false)) {
            const y = py(value);
            ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
        }
        for (let time = first; time <= endTime; time += interval) {
            const x = px(time);
            ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke();
        }
        ctx.setLineDash([]);
        ctx.globalAlpha = 0.45;
        ctx.lineWidth = 1;
        ctx.strokeRect(left, top, right - left, bottom - top);
        ctx.restore();
    }
    function toggle(key) {
        const keys = hiddenKeys.slice();
        const index = keys.indexOf(key);
        if (index >= 0) keys.splice(index, 1); else keys.push(key);
        if (key === "airtmp_max" || key === "wchill_max" || key === "grdtmp_max") {
            const lowKey = key.replace("_max", "_min");
            const lowIndex = keys.indexOf(lowKey);
            if (index >= 0 && lowIndex >= 0) keys.splice(lowIndex, 1);
            if (index < 0 && lowIndex < 0) keys.push(lowKey);
        }
        if (key === "storm_max" || key === "flash_max") {
            const other = key === "storm_max" ? "flash_max" : "storm_max";
            const otherIndex = keys.indexOf(other);
            if (index >= 0 && otherIndex >= 0) keys.splice(otherIndex, 1);
            if (index < 0 && otherIndex < 0) keys.push(other);
        }
        hiddenKeys = keys;
    }
    function calculateBounds(right) {
        const kind = right ? rightScale : scaleKind;
        if (right && kind === "linked") return configuredAxis(bounds, true);
        if (kind === "probability") return configuredAxis(ChartAnalysis.axis(0, 100, 20), right);
        if (kind === "cloudHeights") return configuredAxis(ChartAnalysis.axis(0, 15, 1), right);
        if (kind === "visibility") return configuredAxis(ChartAnalysis.axis(0, 100, 25), right);
        if (kind === "fraction" || kind === "clouds") return configuredAxis(ChartAnalysis.axis(0, 100, 25), right);
        let low = Infinity, high = -Infinity;
        for (const field of fields) {
            if (["hidden", "direction", "type", "storm", "flash"].indexOf(field.kind) >= 0) continue;
            if (isRightField(field) !== Boolean(right)) continue;
            for (const point of field.points.concat(field.lowerPoints || [])) {
                if (point.time < startTime || point.time > endTime || !valid(point.value)) continue;
                low = Math.min(low, point.value); high = Math.max(high, point.value);
            }
        }
        if (!isFinite(low)) { low = 0; high = 1; }
        if (kind === "humidity") return configuredAxis(ChartAnalysis.humidityAxis(low, high), right);
        if (kind === "precipitation") return configuredAxis(ChartAnalysis.precipitationAxis(high), right);
        if (kind === "wind") {
            const speeds = valuesFor("wind10_sd_true_prev_point"), gusts = valuesFor("wind_gust_max");
            const speedMax = speeds.length ? Math.max.apply(null, speeds) : 0;
            const gustMax = gusts.length ? Math.max.apply(null, gusts) : 0;
            const maximum = speeds.length ? Math.min(Math.max(speedMax, gustMax), speedMax * 4) : gustMax;
            return configuredAxis(ChartAnalysis.windAxis(maximum / windScale, windScale), right);
        }
        if (kind === "temperature") return configuredAxis(ChartAnalysis.niceAxis(low - 1, high + 1), right);
        if (kind === "pressure") return configuredAxis(ChartAnalysis.niceAxis(Math.round(low - Math.max(0, 15 - (high - low)) - 2), Math.round(high + 2)), right);
        return configuredAxis(ChartAnalysis.niceAxis(Math.min(0, low), Math.max(1, high)), right);
    }
    function valuesFor(key) {
        const field = fields.find(f => f.key === key);
        return field ? field.points.filter(p => p.time >= startTime && p.time <= endTime && valid(p.value)).map(p => p.value) : [];
    }
    function tickLabel(value, right) {
        const kind = right ? rightScale : scaleKind;
        if (kind === "fraction") return (value / 100).toFixed(2);
        if (kind === "clouds") return (value * 8 / 100).toFixed(0);
        if (kind === "cloudHeights") return value.toFixed(1);
        const decimals = Math.abs(value - Math.round(value)) < 0.0001 ? 0 : 1;
        return Number(value).toLocaleString(Qt.locale(), 'f', decimals);
    }
    function px(time) { return leftMargin + (time - startTime) / Math.max(1, endTime - startTime) * (plot.width - leftMargin - rightMargin); }
    function py(value, right) {
        const kind = right ? rightScale : scaleKind, prefix = right ? "right" : "left";
        if ((kind === "cloudHeights" || kind === "visibility") && !valid(chartConfiguration[prefix + "Min"]) && !valid(chartConfiguration[prefix + "Max"])) {
            const levels = kind === "visibility" ? [0,1,5,20,100] : [0,0.5,2,7,15];
            let index = 0;
            while (index < levels.length - 2 && value > levels[index + 1]) index++;
            const fraction = (index + Math.max(0, Math.min(1, (value - levels[index]) / (levels[index + 1] - levels[index])))) / 4;
            return plot.height - bottomMargin - fraction * (plot.height - topMargin - bottomMargin);
        }
        const b = right ? rightBounds : bounds;
        return topMargin + (b.high - value) / Math.max(0.0001, b.high - b.low) * (plot.height - topMargin - bottomMargin);
    }
    function ticks(right) {
        const kind = right ? rightScale : scaleKind, prefix = right ? "right" : "left";
        if (!valid(chartConfiguration[prefix + "Min"]) && !valid(chartConfiguration[prefix + "Max"])) {
            if (kind === "cloudHeights") return [0,0.5,2,7,15];
            if (kind === "visibility") return [0,1,5,20,100];
        }
        const b = right ? rightBounds : bounds;
        if (b.ticks) return b.ticks;
        return ChartAnalysis.axis(b.low, b.high, b.step).ticks;
    }
    function secondaryScale() {
        if (scaleKind === "temperature" || scaleKind === "probability") return 1;
        if (scaleKind === "pressure") return 0.75;
        return weather.windUnit === "m/s" ? 3.6 : weather.windUnit === "km/h" ? 1/3.6 : 1/1.943844492;
    }
    function axisUnit(right) {
        if (directionOnly || right && !hasRightAxis) return "";
        const kind = right ? rightScale : scaleKind;
        if (kind === "clouds") return I18n.i18n("octas");
        if (kind === "fraction") return I18n.i18n("fraction");
        if (right && kind === "linked") {
            if (scaleKind === "pressure") return "mm Hg";
            if (wind) return weather.windUnit === "m/s" ? "km/h" : "m/s";
            return axisUnit(false);
        }
        const configured = chartConfiguration[right ? "rightUnit" : "leftUnit"];
        if (configured !== undefined) return configured;
        const field = fields.find(f => isRightField(f) === Boolean(right));
        return field ? field.unit : "";
    }
    function pointAt(list, time) {
        // Exact native timestamp lookup for range partners. Sampling is done
        // once in C++, shared by the inspector, cursor and CSV exporter.
        let low = 0, high = list.length - 1;
        while (low <= high) {
            const middle = Math.floor((low + high) / 2), point = list[middle];
            if (point.time === time) return point;
            if (point.time < time) low = middle + 1; else high = middle - 1;
        }
        return null;
    }
    FontMetrics { id: axisFont; font.family: "sans-serif"; font.pixelSize: 10 }
    function timeAxisLabels() {
        return Style.timeAxisLabels(leftMargin, plot.width - rightMargin, startTime, endTime,
            weather.firstTimestamp, (time, format) => weather.formatTime(time, format), text => axisFont.advanceWidth(text), weather.calendarDays(startTime, endTime));
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: root.title; font.bold: true; Layout.maximumWidth: Math.max(0, parent.width - 40); elide: Text.ElideRight }
        AbstractButton {
            id: legendToggle
            opacity: root.exportMode ? 0 : 1
            enabled: !root.exportMode
            text: root.legendVisible ? I18n.i18n("Hide legend") : I18n.i18n("Show legend")
            implicitWidth: 22
            implicitHeight: 22
            hoverEnabled: true
            Accessible.name: text
            contentItem: Item {
                Rectangle {
                    anchors.centerIn: parent
                    width: 14; height: 14; radius: 7
                    color: "transparent"
                    border.width: 1
                    border.color: Kirigami.Theme.textColor
                    opacity: legendToggle.hovered ? 0.9 : root.legendVisible ? 0.6 : 0.35
                    Text { anchors.centerIn: parent; text: "i"; font.family: "serif"; font.pixelSize: 11; font.bold: true; color: Kirigami.Theme.textColor }
                }
            }
            background: Rectangle { radius: 4; color: legendToggle.hovered ? Kirigami.Theme.alternateBackgroundColor : "transparent"; border.width: legendToggle.visualFocus ? 1 : 0; border.color: Kirigami.Theme.highlightColor }
            checkable: true
            checked: root.legendVisible
            onClicked: root.legendOverride = root.legendVisible ? 0 : 1
            ToolTip.text: text
            ToolTip.visible: hovered
        }
        Item { Layout.fillWidth: true }
    }
    Flow {
        visible: root.legendVisible
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: root.fields.filter(f => ["hidden", "type"].indexOf(f.kind) < 0 && (f.kind !== "flash" || !root.fields.some(g => g.kind === "storm")))
            delegate: AbstractButton {
                id: legend
                required property var modelData
                text: modelData.kind === "upper" ? I18n.i18n("Min–max range") : modelData.kind === "feelUpper" ? I18n.i18n("Feels like min–max") : modelData.kind === "groundHigh" ? I18n.i18n("Ground min–max") : modelData.kind === "storm" || modelData.kind === "flash" ? I18n.i18n("Storms / lightning") : modelData.label
                checkable: true
                checked: root.shown(modelData)
                padding: 4
                hoverEnabled: true
                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                implicitHeight: 26
                font.pixelSize: 11
                onClicked: root.toggle(modelData.key)
                background: Rectangle {
                    radius: 4
                    color: !root.exportMode && (legend.hovered || legend.down) ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                    border.width: !root.exportMode && legend.visualFocus ? 1 : 0
                    border.color: Kirigami.Theme.highlightColor
                }
                contentItem: RowLayout {
                    spacing: 5
                    SeriesLegend { Layout.preferredWidth: 28; Layout.preferredHeight: 16; Layout.alignment: Qt.AlignVCenter; seriesStyle: legend.modelData; opacity: legend.checked ? 1 : 0.25 }
                    Label { Layout.alignment: Qt.AlignVCenter; text: legend.text; opacity: legend.checked ? 0.8 : 0.35; font.pixelSize: 11 }
                }
            }
        }
        Repeater {
            model: root.fields.some(f => f.kind === "type") ? [
                {label: I18n.i18n("Rain"), color: root.darkPalette ? "#4ade80" : "#00aa00"},
                {label: I18n.i18n("Snow"), color: root.darkPalette ? "#60a5fa" : "#1539c0"},
                {label: I18n.i18n("Sleet"), color: root.darkPalette ? "#e879f9" : "#b72dc1"}
            ] : []
            delegate: RowLayout {
                required property var modelData
                spacing: 5
                height: 26
                Rectangle { Layout.preferredWidth: 10; Layout.preferredHeight: 10; Layout.alignment: Qt.AlignVCenter; color: parent.modelData.color }
                Label { Layout.alignment: Qt.AlignVCenter; text: parent.modelData.label; font.pixelSize: 11; opacity: 0.8 }
            }
        }
        RowLayout {
            visible: root.fields.some(f => (f.kind === "stems" || f.kind === "gusts") && f.points.some(p => p.time >= root.startTime && p.time <= root.endTime && root.valid(p.value) && p.value > (root.isRightField(f) ? root.rightBounds.high : root.bounds.high)))
            spacing: 5
            height: 26
            Rectangle { Layout.preferredWidth: 4; Layout.preferredHeight: 4; Layout.alignment: Qt.AlignVCenter; radius: 2; color: root.darkPalette ? "#ff6b6b" : "#f44336" }
            Label { Layout.alignment: Qt.AlignVCenter; text: root.wind ? I18n.i18n("Gust above scale") : I18n.i18n("Maximum precipitation above scale"); font.pixelSize: 11; opacity: 0.8 }
        }
    }
    Item {
        id: plot
        objectName: "plot-" + root.groupKey
        Layout.fillWidth: true
        Layout.preferredHeight: root.chartHeight + root.directionBandHeight
        property bool softwareFallback: false
        readonly property string activeRenderer: UserPreferences.renderer === "cpu" || softwareFallback ? "cpu" : "gpu"
        // Own the MSAA texture on the regular container, not on a custom
        // scene-graph item; otherwise arrow-only items can leave an empty layer.
        layer.enabled: activeRenderer === "gpu"
        layer.samples: 4
        function requestPaint() {
            if (activeRenderer === "gpu") {
                const target = renderer.item as GpuPlot;
                if (target) target.requestPaint();
            } else {
                const target = renderer.item as Canvas;
                if (target) target.requestPaint();
            }
        }
        function requestCursorPaint() {
            if (activeRenderer === "gpu") {
                const target = renderer.item as GpuPlot;
                if (target) target.requestCursorPaint();
            } else {
                const target = renderer.item as Canvas;
                if (target) target.requestPaint();
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Loader {
            id: renderer
            anchors.fill: parent
            sourceComponent: plot.activeRenderer === "gpu" ? gpuComponent : cpuComponent
            onLoaded: plot.requestPaint()
        }
        Component {
            id: gpuComponent
            GpuPlot {
                id: gpuPlot
                objectName: "gpu-base-" + root.groupKey
                onPaint: { Style.paintChart(root, plot, getContext("2d"), false); gpuCursor.requestPaint(); }
                onCursorPaintRequested: gpuCursor.requestPaint()
                onSoftwareBackend: plot.softwareFallback = true
                Repeater {
                    model: gpuPlot.textLabels
                    delegate: Text {
                        required property var modelData
                        text: modelData.text
                        color: modelData.color
                        renderType: Text.NativeRendering
                        font: gpuPlot.labelFont
                        x: modelData.x - (modelData.align === "center" ? implicitWidth / 2 : modelData.align === "right" ? implicitWidth : 0)
                        y: modelData.y - (modelData.baseline === "middle" ? implicitHeight / 2 : implicitHeight)
                    }
                }
                GpuPlot {
                    id: gpuCursor
                    objectName: "gpu-cursor-" + root.groupKey
                    anchors.fill: parent
                    onPaint: Style.paintCursor(root, plot, getContext("2d"), true)
                }
            }
        }
        Component {
            id: cpuComponent
            Canvas {
                objectName: "cpu-base-" + root.groupKey
                renderTarget: Canvas.Image
                onPaint: Style.paintChart(root, plot, getContext("2d"))
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: !root.exportMode
            hoverEnabled: root.identificationEnabled
            cursorShape: root.identificationEnabled ? Qt.CrossCursor : Qt.ArrowCursor
            onPositionChanged: mouse => {
                if (!root.identificationEnabled) return;
                if (mouse.x < root.leftMargin || mouse.x > width - root.rightMargin) return;
                const time = root.startTime + (mouse.x - root.leftMargin) / Math.max(1, width - root.leftMargin - root.rightMargin) * (root.endTime - root.startTime);
                root.probeMoved(root.weather.nearestTimestamp(time), mapToItem(null, mouse.x, mouse.y));
            }
            onExited: root.probeLeft()
            onClicked: mouse => {
                if (!root.identificationEnabled) return;
                if (mouse.x < root.leftMargin || mouse.x > width - root.rightMargin) return;
                const time = root.startTime + (mouse.x - root.leftMargin) / Math.max(1, width - root.leftMargin - root.rightMargin) * (root.endTime - root.startTime);
                root.pinRequested(root.weather.nearestTimestamp(time), mapToItem(null, mouse.x, mouse.y));
            }
            onWheel: wheel => {
                if (wheel.modifiers & Qt.ControlModifier) {
                    const center = root.startTime + (wheel.x - root.leftMargin) / Math.max(1, width - root.leftMargin - root.rightMargin) * (root.endTime - root.startTime);
                    root.zoomRequested(wheel.angleDelta.y > 0 ? 0.7 : 1.4, center); wheel.accepted = true;
                } else wheel.accepted = false;
            }
        }
    }
    Label {
        visible: root.legendVisible && root.fields.some(f => f.key === "pcpttl_aver" || f.key === "pcpttl_max")
        Layout.fillWidth: true
        text: I18n.i18n("Fill: mean precipitation. Thin stems: maximum precipitation. Total over ") + (root.fields.length && root.fields[0].points.length > 1 ? (root.fields[0].points[1].time - root.fields[0].points[0].time) / 3600 : 1) + " h."
        wrapMode: Text.WordWrap
        font.pixelSize: 10
        opacity: 0.65
    }
    Connections {
        target: root.weather
        function onStateChanged() { root.nowTime = Date.now() / 1000; }
    }
    Connections {
        target: root
        function onFieldsChanged() { plot.requestPaint(); }
        function onChartConfigurationChanged() { plot.requestPaint(); }
        function onRightBoundsChanged() { plot.requestPaint(); }
        function onHiddenKeysChanged() { plot.requestPaint(); }
        function onBoundsChanged() { plot.requestPaint(); }
        function onSelectedTimeChanged() { plot.requestCursorPaint(); }
        function onExportModeChanged() { plot.requestCursorPaint(); }
        function onStartTimeChanged() { plot.requestPaint(); }
        function onEndTimeChanged() { plot.requestPaint(); }
        function onTextColorChanged() { plot.requestPaint(); }
        function onGridColorChanged() { plot.requestPaint(); }
        function onBackgroundColorChanged() { plot.requestPaint(); }
        function onNightsChanged() { plot.requestPaint(); }
        function onNowTimeChanged() { plot.requestCursorPaint(); }
    }
    Timer { interval: 60000; repeat: true; running: true; onTriggered: root.nowTime = Date.now() / 1000 }
}
