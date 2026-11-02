pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami

Canvas {
    id: root
    required property var seriesStyle
    implicitWidth: 42
    implicitHeight: 24
    property bool darkPalette: Kirigami.Theme.backgroundColor.hslLightness < 0.5
    onSeriesStyleChanged: requestPaint()
    onDarkPaletteChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d"); ctx.reset();
        const style = seriesStyle, kind = style.kind || style.render || "line";
        const colorKey = darkPalette ? "darkColor" : "color", color = style[colorKey] || style.color;
        const left = 2, right = width - 2, middle = height / 2, bottom = height - 3;
        const colors = style.colors || {};
        ctx.strokeStyle = color; ctx.fillStyle = color;
        ctx.lineWidth = Math.min(height / 3, style.lineWidth || 2);
        ctx.globalAlpha = style.opacity === undefined ? 1 : style.opacity;
        function curve(offset) {
            ctx.moveTo(left, middle + offset + 3);
            ctx.bezierCurveTo(width * 0.3, middle + offset - 10, width * 0.6, middle + offset + 8, right, middle + offset - 3);
        }
        if (kind === "range") {
            ctx.beginPath(); curve(-3);
            ctx.lineTo(right, middle + 4);
            ctx.bezierCurveTo(width * 0.6, middle + 15, width * 0.3, middle - 3, left, middle + 10);
            ctx.closePath(); ctx.fill();
        } else if (kind === "area" || kind === "cloudArea") {
            ctx.beginPath(); curve(0); ctx.lineTo(right, bottom); ctx.lineTo(left, bottom); ctx.closePath();
            if (style.legendBands && style.legendBands.length) {
                ctx.save(); ctx.clip();
                for (const band of style.legendBands) {
                    ctx.fillStyle = band[colorKey];
                    ctx.fillRect(left + band.from * (right-left), 0, (band.to-band.from) * (right-left) + 0.5, height);
                }
                ctx.restore();
            } else if (style.fill !== "solid" && style.palette && style.palette.length) {
                ctx.save(); ctx.clip();
                for (let i = 0; i < style.palette.length; ++i) {
                    ctx.fillStyle = style.palette[i][darkPalette ? 2 : 1];
                    ctx.fillRect(left + i * (right-left) / style.palette.length, 0, (right-left) / style.palette.length + 0.5, height);
                }
                ctx.restore();
            } else ctx.fill();
            ctx.beginPath(); curve(0); ctx.stroke();
        } else if (kind === "bars" || kind === "type") {
            const samples = [color, colors.snow ? colors.snow[colorKey] : color, colors.sleet ? colors.sleet[colorKey] : color];
            for (let i = 0; i < 5; ++i) {
                ctx.fillStyle = kind === "type" ? samples[i % 3] : color;
                const h = [5, 12, 17, 13, 8][i] * height / 24;
                ctx.fillRect(left + i * (right-left) / 5, bottom - h, (right-left) / 5 - 1, h);
            }
        } else if (["rangeStems", "stems", "rightBars"].indexOf(kind) >= 0) {
            ctx.beginPath();
            for (let i = 0; i < 5; ++i) {
                const x = 4 + i * (width - 8) / 4, top = [8, 3, 5, 9, 6][i] * height / 24;
                ctx.moveTo(x, top); ctx.lineTo(x, kind === "rangeStems" ? Math.min(bottom, top + height * 0.5) : bottom);
            }
            ctx.stroke();
        } else if (kind === "storm" || kind === "flash") {
            const names = ["low", "medium", "high"];
            for (let i = 0; i < 3; ++i) {
                ctx.fillStyle = style.eventColors !== false && colors[names[i]] ? colors[names[i]][colorKey] : color;
                ctx.strokeStyle = colors.outline ? colors.outline[colorKey] : color;
                ctx.beginPath(); ctx.arc(6 + i * (width - 12) / 2, middle, Math.min(4, style.pointSize || 4), 0, Math.PI * 2);
                if (kind === "flash" || style.legendHasLightning === true) ctx.fill();
                ctx.stroke();
            }
        } else if (kind === "points" || kind === "cloudPoint") {
            for (let i = 0; i < 4; ++i) {
                ctx.beginPath(); ctx.arc(4 + i * (width - 8) / 3, middle + [3, -3, 1, -2][i], Math.min(5, style.pointSize || 2.5), 0, Math.PI * 2); ctx.fill();
                if (kind === "cloudPoint") { ctx.lineWidth = 0.5; ctx.strokeStyle = Kirigami.Theme.textColor; ctx.stroke(); }
            }
        } else if (kind === "direction") {
            for (let i = 0; i < 3; ++i) {
                ctx.save(); ctx.translate(6 + i * (width - 12) / 2, middle); ctx.rotate((-35 + 30 * i) * Math.PI / 180);
                ctx.beginPath(); ctx.moveTo(0, 6); ctx.lineTo(0, -6); ctx.moveTo(-3, -2); ctx.lineTo(0, -6); ctx.lineTo(3, -2); ctx.stroke(); ctx.restore();
            }
        } else if (kind === "gusts") {
            ctx.beginPath();
            for (let i = 0; i < 3; ++i) {
                const x = left + i * (right-left) / 3, y = middle + [3, -4, 0][i];
                ctx.moveTo(x, y); ctx.lineTo(x + (right-left) / 3 - 2, y);
            }
            ctx.stroke();
        } else {
            ctx.setLineDash(style.lineStyle === "dotted" ? [1, 3] : style.lineStyle === "dashed" ? [4, 3] : []);
            ctx.beginPath(); curve(0); ctx.stroke();
        }
    }
}
