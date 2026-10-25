pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    required property string title
    required property string groupKey
    required property var fields
    property real startTime: weather.firstTimestamp
    property real endTime: weather.lastTimestamp
    property real selectedTime: 0
    property var nights: []
    property var hiddenKeys: []
    property int chartHeight: directionOnly ? 64 : 174
    property real nowTime: Date.now() / 1000
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color gridColor: Kirigami.Theme.disabledTextColor
    readonly property color backgroundColor: Kirigami.Theme.backgroundColor
    readonly property bool darkPalette: backgroundColor.hslLightness < 0.5
    readonly property real leftMargin: 50
    readonly property real rightMargin: hasRightAxis ? 44 : 14
    readonly property bool hasRightAxis: fields.some(f => f.kind === "right" || f.kind === "rightBars") || groupKey === "pressure" || groupKey === "wind"
    readonly property bool wind: groupKey === "wind"
    readonly property bool directionOnly: groupKey === "windDirection"
    readonly property real topMargin: directionOnly ? 8 : 28
    readonly property real bottomMargin: directionOnly ? 8 : 30
    readonly property var bounds: calculateBounds()
    readonly property var rightBounds: groupKey === "storms" ? calculateBounds(true) : ({low: 0, high: 100, step: 25})
    signal probeMoved(real timestamp, point position)
    signal probeLeft()
    signal pinRequested()
    signal zoomRequested(real factor, real center)
    spacing: 6
    objectName: "chart-" + groupKey

    function valid(value) { return value !== null && value !== undefined && isFinite(value); }
    function shown(field) { return hiddenKeys.indexOf(field.key) < 0; }
    function seriesColor(field) { return darkPalette && field.darkColor ? field.darkColor : field.color; }
    function layerOrder(field) {
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
        ctx.strokeStyle = gridColor;
        ctx.globalAlpha = 0.28;
        ctx.lineWidth = 1;
        ctx.setLineDash([1.5, 2]);
        if (!directionOnly) for (const value of ticks(false)) {
            const y = py(value);
            ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
        }
        for (let time = first; time <= endTime; time += interval) {
            const x = px(time);
            ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke();
        }
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
        hiddenKeys = keys;
    }
    function calculateBounds(right) {
        if (!right && groupKey === "probability") return {low: 0, high: 100, step: 25};
        if (!right && groupKey === "cloudHeights") return {low: 0, high: 15, step: 1};
        let low = Infinity, high = -Infinity;
        for (const field of fields) {
            if (["hidden", "direction", "type", "storm", "flash"].indexOf(field.kind) >= 0) continue;
            if ((field.kind === "right" || field.kind === "rightBars") !== Boolean(right)) continue;
            for (const point of field.points) {
                if (point.time < startTime || point.time > endTime || !valid(point.value)) continue;
                low = Math.min(low, point.value); high = Math.max(high, point.value);
            }
        }
        if (!isFinite(low)) return {low: 0, high: 1, step: 0.25};
        if (["precipitation", "wind", "clouds", "cloudHeights", "humidity", "visibility", "storms"].indexOf(groupKey) >= 0) low = 0;
        if (["clouds", "humidity"].indexOf(groupKey) >= 0) return {low: 0, high: 100, step: 25};
        const range = Math.max(high - low, groupKey === "pressure" ? 4 : 2);
        const raw = range / 4;
        const magnitude = Math.pow(10, Math.floor(Math.log10(raw)));
        const normalized = raw / magnitude;
        const step = (normalized <= 1 ? 1 : normalized <= 2 ? 2 : normalized <= 2.5 ? 2.5 : normalized <= 5 ? 5 : 10) * magnitude;
        const lower = Math.floor(low / step) * step;
        return {low: lower, high: Math.max(lower + step, Math.ceil((high + range * 0.04) / step) * step), step: step};
    }
    function px(time) { return leftMargin + (time - startTime) / Math.max(1, endTime - startTime) * (plot.width - leftMargin - rightMargin); }
    function py(value, right) {
        if (groupKey === "cloudHeights") {
            const levels = right ? [0,1,5,20,100] : [0,0.5,2,7,15];
            let index = 0;
            while (index < levels.length - 2 && value > levels[index + 1]) index++;
            const fraction = (index + Math.max(0, Math.min(1, (value - levels[index]) / (levels[index + 1] - levels[index])))) / 4;
            return plot.height - bottomMargin - fraction * (plot.height - topMargin - bottomMargin);
        }
        const b = right ? rightBounds : bounds;
        return topMargin + (b.high - value) / Math.max(0.0001, b.high - b.low) * (plot.height - topMargin - bottomMargin);
    }
    function ticks(right) {
        if (groupKey === "cloudHeights") return right ? [0,1,5,20,100] : [0,0.5,2,7,15];
        const b = right ? rightBounds : bounds, result = [];
        for (let value = b.low; value <= b.high + 0.0001; value += b.step) result.push(value);
        return result;
    }
    function secondaryScale() {
        if (groupKey === "pressure") return 0.750061683;
        return weather.windUnit === "m/s" ? 3.6 : weather.windUnit === "km/h" ? 1/3.6 : 1/1.943844492;
    }
    function pointAt(list, time) {
        if (!list.length) return null;
        const step = list.length > 1 ? list[1].time - list[0].time : 3600;
        const index = Math.round((time - list[0].time) / step);
        return index >= 0 && index < list.length ? list[index] : null;
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: root.title; font.bold: true; Layout.fillWidth: true }
        Label {
            text: root.directionOnly ? "skąd wieje" : root.groupKey === "precipitation" ? "mm · wilgotność %" : root.groupKey === "clouds" ? "oktanty · mgła %" : root.groupKey === "pressure" ? "hPa · mm Hg" : root.wind ? root.weather.windUnit + " · " + (root.weather.windUnit === "m/s" ? "km/h" : "m/s") : root.fields.length ? root.fields[0].unit : ""
            opacity: 0.65; font.pixelSize: 11
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: root.fields.filter(f => ["hidden", "lower", "feelLower", "direction", "type", "storm", "flash"].indexOf(f.kind) < 0 && (f.kind !== "groundLow" || !root.fields.some(g => g.kind === "groundHigh")))
            delegate: AbstractButton {
                id: legend
                required property var modelData
                text: modelData.kind === "upper" ? "Zakres min–max" : modelData.kind === "feelUpper" ? "Odczuwalna min–max" : modelData.kind === "groundHigh" ? "Grunt min–max" : modelData.label
                checkable: true
                checked: root.shown(modelData)
                padding: 4
                hoverEnabled: true
                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                implicitHeight: Math.max(24, implicitContentHeight + topPadding + bottomPadding)
                font.pixelSize: 11
                onClicked: root.toggle(modelData.key)
                background: Rectangle {
                    radius: 4
                    color: legend.hovered || legend.down ? Kirigami.Theme.alternateBackgroundColor : "transparent"
                    border.width: legend.visualFocus ? 1 : 0
                    border.color: Kirigami.Theme.highlightColor
                }
                contentItem: Row {
                    spacing: 5
                    Item {
                        width: 18; height: 14
                        Rectangle {
                            anchors.centerIn: parent
                            width: legend.modelData.kind === "points" ? 4 : legend.modelData.kind === "stems" ? 2 : legend.modelData.kind === "bars" ? 10 : 16
                            height: legend.modelData.kind === "points" ? 4 : ["stems", "bars", "upper", "feelUpper", "groundHigh"].indexOf(legend.modelData.kind) >= 0 ? 10 : 3
                            radius: legend.modelData.kind === "points" ? 2 : 1
                            color: root.seriesColor(legend.modelData)
                            opacity: legend.checked ? 1 : 0.25
                            border.width: color.hslLightness > 0.9 ? 0.5 : 0
                            border.color: root.gridColor
                        }
                    }
                    Label { text: legend.text; opacity: legend.checked ? 0.8 : 0.35; font: legend.font }
                }
            }
        }
    }
    Canvas {
        id: plot
        objectName: "plot-" + root.groupKey
        Layout.fillWidth: true
        Layout.preferredHeight: root.chartHeight
        renderTarget: Canvas.Image
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d"); ctx.reset();
            const left = root.leftMargin, right = width - root.rightMargin;
            const top = root.topMargin, bottom = height - root.bottomMargin;
            if (right <= left || bottom <= top) return;
            if (root.groupKey === "clouds" || root.groupKey === "cloudHeights") {
                ctx.fillStyle = root.darkPalette ? "#1e3a5f" : "#87cefa";
                ctx.fillRect(left, top, right - left, bottom - top);
            }
            if ((root.groupKey === "temperature" || root.groupKey === "ground") && root.bounds.low < 0) {
                const zero = Math.max(top, Math.min(bottom, root.py(0)));
                ctx.fillStyle = root.darkPalette ? "#2c5f7c" : "#87ceeb";
                ctx.fillRect(left, zero, right - left, bottom - zero);
            }
            ctx.font = "10px sans-serif";
            ctx.textBaseline = "middle";
            ctx.save(); ctx.beginPath(); ctx.rect(left, top, right - left, bottom - top); ctx.clip();
            for (const night of root.nights) {
                ctx.fillStyle = root.textColor; ctx.globalAlpha = 0.045;
                ctx.fillRect(root.px(night.start), top, root.px(night.end) - root.px(night.start), bottom - top);
            }
            ctx.restore();
            ctx.fillStyle = root.textColor;
            ctx.textAlign = "right";
            const decimals = root.bounds.step < 1 ? 1 : 0;
            for (const value of root.directionOnly ? [] : root.ticks(false)) {
                const y = root.py(value);
                ctx.globalAlpha = 0.8;
                const labelValue = root.groupKey === "clouds" ? value * 8 / 100 : value;
                ctx.fillText(Math.abs(labelValue) < 0.0001 ? "0" : Number(labelValue).toLocaleString(Qt.locale(), 'f', value === 0.5 ? 1 : decimals), left - 8, y);
            }
            if (root.hasRightAxis) {
                ctx.textAlign = "left";
                const linked = root.groupKey === "pressure" || root.wind;
                const rightField = root.fields.find(f => f.kind === "right" || f.kind === "rightBars");
                if (rightField) ctx.fillStyle = root.seriesColor(rightField);
                for (const value of root.ticks(!linked))
                    ctx.fillText((linked ? value * root.secondaryScale() : value).toFixed(0), right + 6, root.py(value, !linked));
            }
            const span = (root.endTime - root.startTime) / 3600;
            const hours = span <= 30 ? 3 : span <= 72 ? 6 : span <= 150 ? 12 : 24;
            const offset = root.weather.utc ? 0 : -new Date(root.startTime * 1000).getTimezoneOffset() * 60;
            const first = Math.ceil((root.startTime + offset) / (hours * 3600)) * hours * 3600 - offset;
            ctx.textAlign = "center";
            let lastDay = "";
            for (let time = first; time <= root.endTime; time += hours * 3600) {
                const x = root.px(time);
                ctx.fillStyle = root.textColor; ctx.globalAlpha = 0.75;
                if (!root.directionOnly) ctx.fillText(root.weather.formatTime(time, "HH:mm"), Math.min(right - 14, Math.max(left + 14, x)), bottom + 15);
                const day = root.weather.formatTime(time, "ddd d MMM");
                if (!root.directionOnly && day !== lastDay) { ctx.globalAlpha = 0.8; ctx.fillText(day, Math.max(left + 32, Math.min(right - 32, x)), 12); lastDay = day; }
            }
            ctx.save(); ctx.beginPath(); ctx.rect(left, top, right - left, bottom - top); ctx.clip();
            for (const pair of [["groundLow", "groundHigh"], ["feelLower", "feelUpper"], ["lower", "upper"]]) {
              const lower = root.fields.find(f => f.kind === pair[0] && root.shown(f));
              const upper = root.fields.find(f => f.kind === pair[1] && root.shown(f));
              if (lower && upper) {
                ctx.fillStyle = root.seriesColor(upper); ctx.globalAlpha = pair[0] === "lower" ? 0.32 : 0.18;
                for (let i = 1; i < lower.points.length && i < upper.points.length; ++i) {
                    const a = lower.points[i - 1], b = lower.points[i], c = upper.points[i], d = upper.points[i - 1];
                    if (![a.value,b.value,c.value,d.value].every(root.valid)) continue;
                    ctx.beginPath(); ctx.moveTo(root.px(a.time), root.py(a.value)); ctx.lineTo(root.px(b.time), root.py(b.value));
                    ctx.lineTo(root.px(c.time), root.py(c.value)); ctx.lineTo(root.px(d.time), root.py(d.value)); ctx.closePath(); ctx.fill();
                }
                if (pair[0] === "groundLow") {
                    ctx.strokeStyle = root.seriesColor(upper); ctx.globalAlpha = 0.65; ctx.lineWidth = 0.8;
                    for (const boundary of [lower, upper]) {
                        ctx.beginPath(); let started = false;
                        for (const point of boundary.points) {
                            if (!root.valid(point.value)) { started = false; continue; }
                            if (!started) { ctx.moveTo(root.px(point.time), root.py(point.value)); started = true; }
                            else ctx.lineTo(root.px(point.time), root.py(point.value));
                        }
                        ctx.stroke();
                    }
                }
              }
            }
            const ordered = root.fields.map((field, index) => ({field: field, index: index}))
                .sort((a,b) => root.layerOrder(a.field) - root.layerOrder(b.field) || a.index - b.index).map(entry => entry.field);
            for (const field of ordered) {
                if (!root.shown(field) || ["hidden", "lower", "upper", "feelLower", "feelUpper", "direction", "type", "storm", "flash"].indexOf(field.kind) >= 0) continue;
                if (["groundLow", "groundHigh"].indexOf(field.kind) >= 0 && root.fields.some(f => f.kind === "groundLow" && root.shown(f)) && root.fields.some(f => f.kind === "groundHigh" && root.shown(f))) continue;
                const isRight = field.kind === "right" || field.kind === "rightBars";
                const step = field.points.length > 1 ? field.points[1].time - field.points[0].time : 3600;
                const barWidth = field.kind === "rightBars" ? 3 : Math.max(2, Math.abs(root.px(root.startTime + step) - left) * (field.kind === "barsBack" ? 0.9 : 0.8));
                ctx.strokeStyle = root.seriesColor(field); ctx.fillStyle = root.seriesColor(field); ctx.lineWidth = field.key === "airtmp_point" ? 2.4 : 1.6;
                ctx.setLineDash(field.kind === "dashed" ? [4,3] : []);
                ctx.globalAlpha = field.kind === "barsBack" ? 0.4 : 1;
                if (field.kind === "cloudArea" || field.key === "slpres_point" || field.key === "wind10_sd_true_prev_point") {
                    for (let i = 1; i < field.points.length; ++i) {
                        const a = field.points[i-1], b = field.points[i];
                        if (!root.valid(a.value) || !root.valid(b.value)) continue;
                        const average = (a.value + b.value) / 2;
                        ctx.fillStyle = field.kind === "cloudArea" ? root.seriesColor(field)
                            : field.key === "slpres_point" ? Qt.hsla(Math.max(0, Math.min(0.42, (1030-average)/60)), 1, 0.5, 1)
                            : Qt.hsla(Math.max(0, Math.min(0.66, 0.66-average/35)), 1, 0.5, 1);
                        ctx.beginPath(); ctx.moveTo(root.px(a.time), bottom); ctx.lineTo(root.px(a.time), root.py(a.value));
                        ctx.lineTo(root.px(b.time), root.py(b.value)); ctx.lineTo(root.px(b.time), bottom); ctx.closePath(); ctx.fill();
                    }
                }
                ctx.beginPath(); let started = false;
                for (const point of field.points) {
                    if (!root.valid(point.value)) { started = false; continue; }
                    const isPrecipitationBar = field.key === "pcpttl_aver" || field.key === "pcpttl_max";
                    const x = root.px(point.time + (isPrecipitationBar ? step / 2 : 0)), y = root.py(point.value, isRight);
                    if (field.kind === "cloudPoint" || field.kind === "points") {
                        if (field.kind === "cloudPoint" && point.value < 0) continue;
                        ctx.beginPath(); ctx.arc(x, y, field.kind === "points" ? 2 : field.key === "cldtop" ? 3.2 : 2.6, 0, Math.PI * 2); ctx.fill();
                    } else if (field.kind === "gusts") {
                        ctx.moveTo(x - barWidth / 2, y); ctx.lineTo(x + barWidth / 2, y);
                    } else if (field.kind === "bars" || field.kind === "barsBack" || field.kind === "stems" || field.kind === "rightBars") {
                        if (isPrecipitationBar) {
                            const types = root.fields.find(f => f.kind === "type");
                            const type = types ? root.pointAt(types.points, point.time) : null;
                            const rainColor = root.seriesColor(field);
                            const snowColor = root.darkPalette ? "#60a5fa" : "#1539c0";
                            const mixedColor = root.darkPalette ? "#e879f9" : "#b72dc1";
                            ctx.fillStyle = type && type.value === 2 ? snowColor : type && type.value === 3 ? mixedColor : rainColor;
                        }
                        if (field.kind === "stems") {
                            ctx.strokeStyle = ctx.fillStyle; ctx.lineWidth = 1.2;
                            ctx.beginPath(); ctx.moveTo(x, root.py(0)); ctx.lineTo(x, y); ctx.stroke();
                        } else {
                            const from = field.key === "pcpttl_aver" ? Math.floor(root.px(point.time)) : x - barWidth / 2;
                            const to = field.key === "pcpttl_aver" ? Math.ceil(root.px(point.time + step)) : from + barWidth;
                            ctx.fillRect(from, y, to - from, Math.max(0, root.py(0, isRight) - y));
                        }
                    } else {
                        if (!started) { ctx.moveTo(x, y); started = true; } else ctx.lineTo(x, y);
                    }
                }
                ctx.stroke(); ctx.setLineDash([]);
            }
            if (root.groupKey === "probability") {
                const storm = root.fields.find(f => f.kind === "storm" && root.shown(f));
                const flashes = root.fields.find(f => f.kind === "flash" && root.shown(f));
                const events = flashes ? flashes.points : storm ? storm.points : [];
                for (const point of events) {
                    const a = storm ? root.pointAt(storm.points, point.time) : null;
                    const b = flashes ? root.pointAt(flashes.points, point.time) : null;
                    const flash = b && root.valid(b.value) ? b.value : 0;
                    if (flash <= 0 && (!a || !root.valid(a.value) || a.value <= 0)) continue;
                    ctx.fillStyle = flash >= 30 ? "#f44336" : flash >= 10 ? "#ff9800" : flash > 0 ? "#ffeb3b" : "white";
                    ctx.strokeStyle = root.textColor; ctx.lineWidth = 1; ctx.globalAlpha = 1;
                    ctx.beginPath(); ctx.arc(root.px(point.time), (top + bottom) / 2, 4, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
                }
            }
            if (root.directionOnly) {
                const directions = root.fields.find(f => f.kind === "direction");
                if (directions) {
                    let lastX = -100;
                    for (const point of directions.points) {
                        const x = root.px(point.time);
                        if (x < left || x > right || x - lastX < 12 || !root.valid(point.value)) continue;
                        lastX = x;
                        ctx.save(); ctx.translate(x, (top + bottom) / 2); ctx.rotate((point.value + 180) * Math.PI / 180);
                        ctx.strokeStyle = root.seriesColor(directions); ctx.globalAlpha = 1; ctx.lineWidth = 1.3;
                        ctx.beginPath(); ctx.moveTo(0,7); ctx.lineTo(0,-7); ctx.moveTo(-3,-3); ctx.lineTo(0,-7); ctx.lineTo(3,-3); ctx.stroke(); ctx.restore();
                    }
                }
            }
            // The ICM grid stays visible over opaque precipitation, wind,
            // pressure and cloud fills. Cursor indicators are the top layer.
            root.drawGrid(ctx, left, right, top, bottom, first, hours * 3600);
            if (root.nowTime >= root.startTime && root.nowTime <= root.endTime) {
                ctx.strokeStyle = root.textColor; ctx.globalAlpha = 0.4; ctx.lineWidth = 1; ctx.setLineDash([4,4]);
                ctx.beginPath(); ctx.moveTo(root.px(root.nowTime), top); ctx.lineTo(root.px(root.nowTime), bottom); ctx.stroke(); ctx.setLineDash([]);
            }
            if (root.selectedTime >= root.startTime && root.selectedTime <= root.endTime) {
                const x = root.px(root.selectedTime);
                ctx.strokeStyle = root.textColor; ctx.globalAlpha = 0.65; ctx.lineWidth = 1;
                ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke();
                for (const field of root.fields) {
                    if (!root.shown(field) || ["hidden", "lower", "upper", "feelLower", "feelUpper", "direction", "type", "barsBack", "storm", "flash"].indexOf(field.kind) >= 0) continue;
                    const point = root.pointAt(field.points, root.selectedTime);
                    if (!point || !root.valid(point.value)) continue;
                    ctx.fillStyle = root.seriesColor(field); ctx.strokeStyle = root.backgroundColor; ctx.lineWidth = 1.5; ctx.globalAlpha = 1;
                    ctx.beginPath(); ctx.arc(x, root.py(point.value, field.kind === "right" || field.kind === "rightBars"), 3.5, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
                }
            }
            ctx.restore();
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.CrossCursor
            onPositionChanged: mouse => {
                if (mouse.x < root.leftMargin || mouse.x > width - root.rightMargin) return;
                const time = root.startTime + (mouse.x - root.leftMargin) / Math.max(1, width - root.leftMargin - root.rightMargin) * (root.endTime - root.startTime);
                root.probeMoved(root.weather.nearestTimestamp(time), mapToItem(null, mouse.x, mouse.y));
            }
            onExited: root.probeLeft()
            onClicked: root.pinRequested()
            onWheel: wheel => {
                if (wheel.modifiers & Qt.ControlModifier) {
                    const center = root.startTime + (wheel.x - root.leftMargin) / Math.max(1, width - root.leftMargin - root.rightMargin) * (root.endTime - root.startTime);
                    root.zoomRequested(wheel.angleDelta.y > 0 ? 0.7 : 1.4, center); wheel.accepted = true;
                } else wheel.accepted = false;
            }
        }
    }
    Label {
        visible: root.groupKey === "precipitation"
        Layout.fillWidth: true
        text: "Wypełnienie: opad średni. Cienkie kreski: opad maksymalny. Suma w przedziale " + (root.fields.length && root.fields[0].points.length > 1 ? (root.fields[0].points[1].time - root.fields[0].points[0].time) / 3600 : 1) + " h."
        wrapMode: Text.WordWrap
        font.pixelSize: 10
        opacity: 0.65
    }
    Connections {
        target: root
        function onFieldsChanged() { plot.requestPaint(); }
        function onHiddenKeysChanged() { plot.requestPaint(); }
        function onBoundsChanged() { plot.requestPaint(); }
        function onSelectedTimeChanged() { plot.requestPaint(); }
        function onStartTimeChanged() { plot.requestPaint(); }
        function onEndTimeChanged() { plot.requestPaint(); }
        function onTextColorChanged() { plot.requestPaint(); }
        function onGridColorChanged() { plot.requestPaint(); }
        function onBackgroundColorChanged() { plot.requestPaint(); }
        function onNightsChanged() { plot.requestPaint(); }
        function onNowTimeChanged() { plot.requestPaint(); }
    }
    Timer { interval: 60000; repeat: true; running: true; onTriggered: root.nowTime = Date.now() / 1000 }
}
