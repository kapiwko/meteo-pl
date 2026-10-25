pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami

Canvas {
    id: root
    required property var seriesStyle
    implicitWidth: 42
    implicitHeight: 18
    readonly property bool darkPalette: Kirigami.Theme.backgroundColor.hslLightness < 0.5
    onSeriesStyleChanged: requestPaint()
    onDarkPaletteChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d"); ctx.reset();
        const style = seriesStyle, kind = style.render || style.kind || "line";
        const color = darkPalette && style.darkColor ? style.darkColor : style.color;
        ctx.strokeStyle = color; ctx.fillStyle = color;
        ctx.lineWidth = style.lineWidth || 2;
        ctx.globalAlpha = style.opacity === undefined ? 1 : style.opacity;
        const middle = height / 2;
        if (kind === "range" || kind === "area" || kind === "cloudArea") {
            ctx.fillRect(2, 3, width - 4, height - 6);
        } else if (kind === "bars") {
            for (let i = 0; i < 4; ++i) ctx.fillRect(2 + i * (width - 4) / 4, 3 + i * 2, (width - 4) / 4, height - 5 - i * 2);
        } else if (["rangeStems", "stems", "rightBars"].indexOf(kind) >= 0) {
            ctx.beginPath();
            for (let i = 0; i < 3; ++i) { const x = 6 + i * (width - 12) / 2; ctx.moveTo(x, 3 + i); ctx.lineTo(x, height - 3); }
            ctx.stroke();
        } else if (["points", "cloudPoint", "storm", "flash"].indexOf(kind) >= 0) {
            for (let i = 0; i < 3; ++i) { ctx.beginPath(); ctx.arc(6 + i * (width - 12) / 2, middle, Math.min(6, style.pointSize || 2.5), 0, Math.PI * 2); ctx.fill(); }
        } else if (kind === "direction") {
            ctx.beginPath(); ctx.moveTo(4,middle); ctx.lineTo(width-5,middle); ctx.moveTo(width-10,middle-4); ctx.lineTo(width-5,middle); ctx.lineTo(width-10,middle+4); ctx.stroke();
        } else if (kind === "type") {
            for (let i = 0; i < 3; ++i) { ctx.fillStyle = ["#00aa00","#1539c0","#b72dc1"][i]; ctx.fillRect(2+i*(width-4)/3,3,(width-4)/3,height-6); }
        } else {
            ctx.setLineDash(style.lineStyle === "dotted" ? [1,3] : style.lineStyle === "dashed" ? [4,3] : []);
            ctx.beginPath(); ctx.moveTo(2,middle); ctx.lineTo(width-2,middle); ctx.stroke();
        }
    }
}
