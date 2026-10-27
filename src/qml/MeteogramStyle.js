.pragma library

// All appearance values come from the chart's JSON configuration.
function paletteColor(stops, value, dark, fallback) {
    for (var i = 0; i < stops.length; ++i)
        if (value < stops[i][0]) return stops[i][dark ? 2 : 1];
    return fallback[dark ? "darkColor" : "color"];
}
function pressureColor(hPa, dark, field) {
    return paletteColor(field.palette, hPa, dark, field.paletteFallback);
}
function windColor(mps, dark, field) {
    return paletteColor(field.palette, mps, dark, field.paletteFallback);
}
function namedColor(field, name, dark) {
    return field.colors[name][dark ? "darkColor" : "color"];
}
function timeAxisLabels(left, right, startTime, endTime, origin, formatTime, measureText, calendarDays) {
    const width = right - left;
    const span = (endTime - startTime) / 3600;
    if (width <= 0 || span <= 0) return {hours: 3, format: "HH", labels: [], days: [], midnights: []};
    let hours = span <= 72 ? 3 : span <= 150 ? 6 : 12;
    const format = width * hours / span >= measureText("00:00") + 16 ? "HH:mm" : "HH";
    const minimumSpacing = measureText(format === "HH" ? "00" : "00:00") + 12;
    while (width * hours / span < minimumSpacing) hours *= 2;
    const first = origin + Math.ceil((startTime - origin) / (hours * 3600)) * hours * 3600;
    const labels = [], days = [], midnights = [];
    let lastRight = -Infinity, lastDayRight = -Infinity;
    for (let time = first; time <= endTime; time += hours * 3600) {
        const text = formatTime(time, format), half = measureText(text) / 2;
        const x = Math.max(left + half, Math.min(right - half, (left + (time - startTime) / (endTime - startTime) * width)));
        if (x - half >= lastRight + 8) {
            labels.push({time: time, text: text, x: x, width: half * 2});
            lastRight = x + half;
        }
    }
    for (const day of calendarDays) {
        const start = Math.max(startTime, day.start), end = Math.min(endTime, day.end);
        const dayWidth = (end - start) / (endTime - startTime) * width;
        let text = formatTime((start + end) / 2, "ddd d MMM");
        if (measureText(text) + 8 > dayWidth) text = formatTime((start + end) / 2, "dd.MM");
        const textWidth = measureText(text), x = left + ((start + end) / 2 - startTime) / (endTime - startTime) * width;
        if (textWidth + 8 <= dayWidth && x - textWidth / 2 >= lastDayRight + 8) {
            days.push({text: text, x: x, width: textWidth, start: start, end: end}); lastDayRight = x + textWidth / 2;
        }
        if (day.start >= startTime) midnights.push(day.start);
        if (day.end === endTime) midnights.push(day.end);
    }
    return {hours: hours, format: format, labels: labels, days: days, midnights: midnights};
}

function paintDirections(root, ctx, left, right, center) {
    const directions = root.fields.find(f => f.kind === "direction" && root.shown(f));
    if (directions) {
        let lastX = -100;
        for (const point of directions.points) {
            const x = root.px(point.time);
            if (x < left || x > right || x - lastX < 12 || !root.valid(point.value)) continue;
            lastX = x;
            ctx.save(); ctx.translate(x, center); ctx.rotate((point.value + 180) * Math.PI / 180);
            ctx.strokeStyle = root.seriesColor(directions); ctx.globalAlpha = directions.opacity === undefined ? 1 : directions.opacity; ctx.lineWidth = directions.lineWidth || 1.5;
            ctx.beginPath(); ctx.moveTo(0,3.5); ctx.lineTo(0,-3.5); ctx.stroke();
            ctx.fillStyle = root.seriesColor(directions);
            ctx.beginPath(); ctx.moveTo(0,-7.5); ctx.lineTo(-3.5,-3.5); ctx.lineTo(3.5,-3.5); ctx.closePath(); ctx.fill(); ctx.restore();
        }
    }
}

function intervalBounds(point) {
    const from = Number.isFinite(point.intervalFrom) ? point.intervalFrom : point.time;
    const duration = Number.isFinite(point.interval) && point.interval > 0 ? point.interval : 0;
    const to = Number.isFinite(point.intervalTo) && point.intervalTo >= from ? point.intervalTo : from + duration;
    return {from: from, to: to, center: (from + to) / 2};
}

function paintChart(root, plot, ctx, withCursor) {
    const width = plot.width, height = plot.height; ctx.reset();
    const left = root.leftMargin, right = width - root.rightMargin;
    const top = root.topMargin, bottom = height - root.bottomMargin;
    if (right <= left || bottom <= top) return;
    ctx.fillStyle = root.plotStyle("background");
    ctx.fillRect(left, top, right - left, bottom - top);
    if (root.scaleKind === "clouds" || root.scaleKind === "cloudHeights") {
        ctx.fillStyle = root.plotStyle("cloudBackground");
        ctx.fillRect(left, top, right - left, bottom - top);
    }
    if (root.scaleKind === "temperature" && root.bounds.low < 0) {
        const zero = Math.max(top, Math.min(bottom, root.py(0)));
        ctx.fillStyle = root.plotStyle("freezingBackground");
        ctx.fillRect(left, zero, right - left, bottom - zero);
    }
    ctx.font = "10px sans-serif";
    ctx.textBaseline = "middle";
    ctx.fillStyle = root.textColor;
    ctx.textAlign = "right";
    ctx.globalAlpha = 0.7;
    if (!root.directionOnly) ctx.fillText(root.axisUnit(false), left - 8, top - 14);
    for (const value of root.directionOnly ? [] : root.ticks(false)) {
        const y = root.py(value);
        ctx.globalAlpha = 0.8;
        ctx.fillText(root.tickLabel(value, false), left - 8, y);
    }
    if (root.hasRightAxis) {
        ctx.textAlign = "left";
        const linked = root.rightScale === "linked";
        const rightField = root.fields.find(f => root.isRightField(f));
        if (rightField) ctx.fillStyle = root.rightScale === "fraction" || root.rightScale === "visibility" ? root.plotStyle("rightAxisAccent") : root.seriesColor(rightField);
        ctx.fillText(root.axisUnit(true), right + 6, top - 14);
        for (const value of root.ticks(!linked))
            ctx.fillText(root.tickLabel(linked ? value * root.secondaryScale() : value, !linked), right + 6, root.py(value, !linked));
    }
    const origin = root.weather.firstTimestamp;
    ctx.textAlign = "center";
    const axis = root.timeAxisLabels();
    if (!root.directionOnly) {
        ctx.fillStyle = root.textColor; ctx.globalAlpha = 0.75;
        for (const label of axis.labels) ctx.fillText(label.text, label.x, bottom + root.directionBandHeight + 15);
        ctx.globalAlpha = 0.8;
        for (const label of axis.days) ctx.fillText(label.text, label.x, 12);
    }
    ctx.save(); ctx.beginPath(); ctx.rect(left, top, right - left, bottom - top); ctx.clip();
    const fields = root.plotFields || root.fields;
    const ranges = fields.filter(f => f.kind === "range" && root.shown(f)).sort((a,b) => root.layerOrder(a) - root.layerOrder(b));
    for (const range of ranges) {
        const isRight = root.isRightField(range);
        ctx.fillStyle = root.seriesColor(range); ctx.globalAlpha = range.opacity === undefined ? 191 / 255 : range.opacity;
        for (const segment of range.rangeSegments || []) {
            const a = segment.from, b = segment.to;
            ctx.beginPath(); ctx.moveTo(root.px(a.time), root.py(a.lower, isRight)); ctx.lineTo(root.px(b.time), root.py(b.lower, isRight));
            ctx.lineTo(root.px(b.time), root.py(b.upper, isRight)); ctx.lineTo(root.px(a.time), root.py(a.upper, isRight)); ctx.closePath(); ctx.fill();
        }
    }
    const ordered = fields.map((field, index) => ({field: field, index: index}))
        .sort((a,b) => root.layerOrder(a.field) - root.layerOrder(b.field) || a.index - b.index).map(entry => entry.field);
    ctx.globalAlpha = 1;
    for (const field of ordered)
        if (field.kind === "cloudArea" && root.shown(field)) root.drawArea(ctx, field, bottom);
    for (const field of ordered) {
        if (!root.shown(field) || ["hidden", "range", "direction", "type", "storm", "flash"].indexOf(field.kind) >= 0) continue;
        if (field.kind === "cloudArea" && field.key === "cldvlow_aver") continue;
        const isRight = root.isRightField(field);
        ctx.strokeStyle = root.seriesColor(field); ctx.fillStyle = root.seriesColor(field); ctx.lineWidth = field.lineWidth === undefined ? 2 : field.lineWidth;
        ctx.setLineDash(field.lineStyle === "dotted" ? [1,3] : field.lineStyle === "dashed" || field.kind === "dashed" ? [4,3] : []);
        ctx.globalAlpha = field.opacity === undefined ? 1 : field.opacity;
        if (field.kind === "rangeStems") {
            ctx.beginPath();
            for (const pair of field.rangePairs || []) {
                const x = root.px(pair.time);
                ctx.moveTo(x, root.py(pair.lower, isRight)); ctx.lineTo(x, root.py(pair.upper, isRight));
            }
            ctx.stroke();
            continue;
        }
        if (field.kind === "area" || field.key === "slpres_point" && field.render === undefined || field.key === "wind10_sd_true_prev_point" && field.render === undefined) root.drawArea(ctx, field, bottom);
        ctx.beginPath(); let started = false;
        for (const point of field.points) {
            if (!root.valid(point.value)) { started = false; continue; }
            const interval = intervalBounds(point);
            const barWidth = field.kind === "rightBars" ? 3 : Math.max(2, Math.abs(root.px(interval.to) - root.px(interval.from)) * (field.kind === "barsBack" ? 0.9 : 0.8));
            const isPrecipitationBar = field.key === "pcpttl_aver" || field.key === "pcpttl_max";
            const x = root.px(isPrecipitationBar || field.kind === "gusts" ? interval.center : point.time), y = root.py(point.value, isRight);
            if (field.kind === "cloudPoint" || field.kind === "points") {
                if (field.kind === "cloudPoint" && point.value <= 0) continue;
                ctx.beginPath(); ctx.arc(x, y, field.pointSize || (field.kind === "points" ? 2 : 3), 0, Math.PI * 2); ctx.fill();
            } else if (field.kind === "gusts") {
                if (point.value <= 0) continue;
                const gustY = Math.max(top, y);
                ctx.beginPath(); ctx.moveTo(root.px(interval.from), gustY); ctx.lineTo(root.px(interval.to), gustY); ctx.stroke();
                if (point.value > (isRight ? root.rightBounds.high : root.bounds.high)) { ctx.beginPath(); ctx.arc(x, top + 2, 2, 0, Math.PI * 2); ctx.fill(); }
            } else if (field.kind === "bars" || field.kind === "barsBack" || field.kind === "stems" || field.kind === "rightBars") {
                if (isPrecipitationBar && field.precipitationColors !== false) {
                    const types = root.fields.find(f => f.kind === "type");
                    const type = types ? point.precipitationType : null;
                    const rainColor = root.seriesColor(field);
                    const snowColor = root.categoryColor(field, "snow");
                    const mixedColor = root.categoryColor(field, "sleet");
                    ctx.fillStyle = type === 2 ? snowColor : type === 3 ? mixedColor : rainColor;
                }
                if (field.kind === "stems") {
                    ctx.strokeStyle = ctx.fillStyle; ctx.lineWidth = field.lineWidth === undefined ? 2.5 : field.lineWidth;
                    ctx.beginPath(); ctx.moveTo(x, root.py(0, isRight)); ctx.lineTo(x, y); ctx.stroke();
                    if (point.value > (isRight ? root.rightBounds.high : root.bounds.high)) {
                        ctx.fillStyle = root.plotStyle("aboveScale");
                        ctx.beginPath(); ctx.arc(x, top + 2, 2, 0, Math.PI * 2); ctx.fill();
                    }
                } else {
                    const from = field.key === "pcpttl_aver" ? Math.floor(root.px(interval.from)) : x - barWidth / 2;
                    const to = field.key === "pcpttl_aver" ? Math.ceil(root.px(interval.to)) : from + barWidth;
                    const zero = root.py(0, isRight);
                    ctx.fillRect(from, Math.min(y, zero), to - from, Math.abs(zero - y));
                }
            } else {
                if (!started) { ctx.moveTo(x, y); started = true; } else ctx.lineTo(x, y);
            }
        }
        if (["points", "cloudPoint", "gusts", "bars", "barsBack", "stems", "rightBars"].indexOf(field.kind) < 0) ctx.stroke();
        ctx.setLineDash([]);
    }
    if (fields.some(f => f.kind === "storm" || f.kind === "flash")) {
        const storm = fields.find(f => f.kind === "storm" && root.shown(f));
        const flashes = fields.find(f => f.kind === "flash" && root.shown(f));
        const events = flashes ? flashes.events : storm ? storm.events : [];
        for (const point of events) {
            const intensity = storm && root.valid(point.storm) ? point.storm : 0;
            const flash = flashes && root.valid(point.flash) ? point.flash : 0;
            if (flash <= 0 && intensity <= 0) continue;
            const eventStyle = flashes || storm, autoColors = eventStyle.eventColors !== false, radius = eventStyle.pointSize || 4;
            ctx.fillStyle = autoColors ? root.categoryColor(eventStyle, flash >= 30 ? "high" : flash >= 10 ? "medium" : "low") : root.seriesColor(eventStyle);
            const eventX = root.px(intervalBounds(point).center), eventY = (top + bottom) / 2;
            ctx.globalAlpha = eventStyle.opacity === undefined ? 1 : eventStyle.opacity;
            if (flash > 0) { ctx.beginPath(); ctx.arc(eventX, eventY, radius, 0, Math.PI * 2); ctx.fill(); }
            ctx.strokeStyle = root.categoryColor(eventStyle, "outline"); ctx.lineWidth = eventStyle.lineWidth || 2;
            ctx.beginPath(); ctx.arc(eventX, eventY, Math.max(1, radius - 1), 0, Math.PI * 2); ctx.stroke();
        }
    }
    if (root.directionOnly) paintDirections(root, ctx, left, right, (top + bottom) / 2);
    // The ICM grid stays visible over opaque precipitation, wind,
    // pressure and cloud fills. Cursor indicators are the top layer.
    const gridFirst = origin + Math.ceil((root.startTime - origin) / 10800) * 10800;
    root.drawGrid(ctx, left, right, top, bottom, gridFirst, 10800);
    for (const night of root.nights) {
        ctx.fillStyle = root.plotStyle("grid"); ctx.globalAlpha = root.plotStyle("nightOpacity");
        ctx.fillRect(root.px(night.start), top, root.px(night.end) - root.px(night.start), bottom - top);
    }
    ctx.restore();
    if (root.directionBandHeight) {
        const bandBottom = bottom + root.directionBandHeight;
        ctx.save(); ctx.beginPath(); ctx.rect(left, bottom, right - left, root.directionBandHeight); ctx.clip();
        ctx.fillStyle = root.plotStyle("background");
        ctx.fillRect(left, bottom, right - left, root.directionBandHeight);
        root.drawGrid(ctx, left, right, bottom, bandBottom, gridFirst, 10800);
        for (const night of root.nights) {
            ctx.fillStyle = root.plotStyle("grid"); ctx.globalAlpha = root.plotStyle("nightOpacity");
            ctx.fillRect(root.px(night.start), bottom, root.px(night.end) - root.px(night.start), root.directionBandHeight);
        }
        paintDirections(root, ctx, left, right, bottom + root.directionBandHeight / 2);
        ctx.restore();
    }
    ctx.save(); ctx.strokeStyle = root.textColor; ctx.globalAlpha = 0.3; ctx.lineWidth = 1; ctx.setLineDash([]);
    for (const time of axis.midnights) {
        const x = root.px(time);
        ctx.beginPath(); ctx.moveTo(x, root.directionOnly ? top : 2); ctx.lineTo(x, bottom + root.directionBandHeight); ctx.stroke();
    }
    ctx.restore();
    if (withCursor !== false) paintCursor(root, plot, ctx, false);
}

function paintCursor(root, plot, ctx, reset) {
    if (reset) ctx.reset();
    const left = root.leftMargin, right = plot.width - root.rightMargin;
    const top = root.topMargin, bottom = plot.height - root.bottomMargin + root.directionBandHeight;
    if (right <= left || bottom <= top) return;
    ctx.save(); ctx.beginPath(); ctx.rect(left, top, right - left, bottom - top); ctx.clip();
    if (root.nowTime >= root.startTime && root.nowTime <= root.endTime) {
        ctx.strokeStyle = root.textColor; ctx.globalAlpha = 0.4; ctx.lineWidth = 1; ctx.setLineDash([4,4]);
        ctx.beginPath(); ctx.moveTo(root.px(root.nowTime), top); ctx.lineTo(root.px(root.nowTime), bottom); ctx.stroke(); ctx.setLineDash([]);
    }
    if (!root.exportMode && root.selectedTime >= root.startTime && root.selectedTime <= root.endTime) {
        const x = root.px(root.selectedTime);
        ctx.strokeStyle = root.textColor; ctx.globalAlpha = 0.65; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke();
        for (const field of root.fields) {
            if (!root.shown(field) || ["hidden", "direction", "type", "barsBack", "storm", "flash"].indexOf(field.kind) >= 0) continue;
            const sample = root.probeSamples[field.key];
            if (!sample || !root.valid(sample.number) || !root.valid(sample.sourceTimestamp)) continue;
            const centered = ["pcpttl_aver", "pcpttl_max"].indexOf(field.key) >= 0 || field.kind === "gusts";
            const sampleTime = sample.sourceTimestamp + (centered && root.valid(sample.intervalTo) ? (sample.intervalTo - sample.intervalFrom) / 2 : 0);
            ctx.fillStyle = root.seriesColor(field); ctx.strokeStyle = root.backgroundColor; ctx.lineWidth = 1.5; ctx.globalAlpha = 1;
            ctx.beginPath(); ctx.arc(root.px(sampleTime), root.py(sample.number, root.isRightField(field)), 3.5, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
        }
    }
    ctx.restore();
}
