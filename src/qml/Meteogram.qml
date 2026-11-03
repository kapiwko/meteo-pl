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
    readonly property var description: chartConfiguration.prepared ? chartConfiguration : ChartAnalysis.describe(fields, chartConfiguration, weather.windUnit, groupKey)
    readonly property var plotFields: description.fields
    property var chartConfiguration: ({})
    readonly property bool axisConflict: Boolean(chartConfiguration.axisConflict)
    property int legendOverride: -1
    property bool exportMode: false
    property bool showHeading: true
    property bool identificationEnabled: false
    property bool touchNavigationEnabled: false
    property bool compactAxes: false
    property bool renderEnabled: true
    onRenderEnabledChanged: if (renderEnabled) plot.requestPaint()
    // The desktop panel shares its exporter and status message across charts.
    // Standalone charts (including Plasma) use the same native save workflow.
    property var exportHandler: null
    property ChartSaveDialog saveDialog: null
    readonly property var legendFields: description.legend
    readonly property bool legendVisible: legendOverride < 0 ? UserPreferences.showLegend : Boolean(legendOverride)
    readonly property string scaleKind: description.leftAxis.kind
    readonly property string rightScale: description.rightAxis.kind
    property real startTime: weather.firstTimestamp
    property real endTime: weather.lastTimestamp
    property real selectedTime: 0
    readonly property var probeSamples: selectedTime > 0 && Object.keys(weather.series).length
        ? ChartAnalysis.probeSamples(weather.detailsAt(selectedTime), plotFields) : ({})
    property var nights: []
    property var hiddenKeys: []
    property int chartHeight: chartConfiguration.height || Math.round((UserPreferences.profileChartHeight || UserPreferences.chartHeight) * (directionOnly ? 64 / 174 : 1))
    property real nowTime: Date.now() / 1000
    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color gridColor: Kirigami.Theme.disabledTextColor
    readonly property color backgroundColor: Kirigami.Theme.backgroundColor
    readonly property bool darkPalette: backgroundColor.hslLightness < 0.5
    readonly property var appearance: chartConfiguration.appearance || UserPreferences.chartAppearance
    readonly property real leftMargin: compactAxes ? axisMargin(false) : 50
    readonly property real rightMargin: compactAxes ? hasRightAxis ? axisMargin(true) : 6 : 44
    readonly property bool hasRightAxis: description.hasRightAxis
    readonly property bool wind: scaleKind === "wind"
    readonly property bool directionOnly: description.directionOnly
    readonly property real directionBandHeight: !directionOnly && plotFields.some(f => f.kind === "direction") ? 30 : 0
    readonly property real topMargin: directionOnly ? 8 : 28
    readonly property real bottomMargin: directionOnly ? 8 : 30 + directionBandHeight
    readonly property var bounds: description.leftAxis.range
    readonly property var rightBounds: description.rightAxis.range
    readonly property real windScale: description.windScale
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
    signal rangeRequested(real first, real last)
    signal resetRangeRequested()
    spacing: 6
    objectName: "chart-" + groupKey

    function renderDescription() {
        return {weather: {firstTimestamp: weather.firstTimestamp},
            axisUnit: axisUnit, backgroundColor: backgroundColor, bottomMargin: bottomMargin, bounds: bounds,
            categoryColor: categoryColor, darkPalette: darkPalette, directionBandHeight: directionBandHeight, directionOnly: directionOnly,
            drawGrid: drawGrid, endTime: endTime, exportMode: exportMode, fields: plotFields,
            hasRightAxis: hasRightAxis, isRightField: isRightField, layerOrder: layerOrder, leftMargin: leftMargin,
            nights: nights, nowTime: nowTime, plotFields: plotFields, plotStyle: plotStyle,
            probeSamples: probeSamples, px: px, py: py, rightBounds: rightBounds,
            rightMargin: rightMargin, rightScale: rightScale, scaleKind: scaleKind,
            selectedTime: selectedTime, seriesColor: seriesColor, shown: shown, startTime: startTime,
            textColor: textColor, tickLabel: tickLabel, ticks: ticks, timeAxisLabels: timeAxisLabels,
            topMargin: topMargin, valid: valid, windScale: windScale
        };
    }
    function valid(value) { return value !== null && value !== undefined && isFinite(value); }
    function exportStyle(configuration) {
        const style = Object.assign({}, configuration);
        for (const key of ["points", "lowerPoints", "fields", "rangeSegments", "rangePairs", "events", "legend", "typeLegend", "eventLegend", "colorScales", "leftAxis", "rightAxis"]) delete style[key];
        return style;
    }
    function shown(field) { return hiddenKeys.indexOf(field.key) < 0; }
    function legendLabel(field) { return field.legendLabel; }
    function saveChart() {
        chartContextMenu.close();
        if (exportHandler) { exportHandler(root); return; }
        if (!saveDialog) saveDialog = saveDialogComponent.createObject(root, {chart: root});
        saveDialog.choose();
    }
    function seriesColor(field) { return darkPalette && field.darkColor ? field.darkColor : field.color; }
    function categoryColor(field, name) { return field.colors[name][darkPalette ? "darkColor" : "color"]; }
    function plotStyle(name) { return appearance[darkPalette ? "dark" : "light"][name]; }
    function isRightField(field) { return field.axis === "right"; }
    function layerOrder(field) { return field.layer; }
    function drawGrid(ctx, left, right, top, bottom, first, interval) {
        ctx.save();
        ctx.strokeStyle = plotStyle("grid");
        ctx.globalAlpha = plotStyle("gridOpacity");
        ctx.lineWidth = plotStyle("gridWidth");
        ctx.setLineDash(plotStyle("gridDash"));
        if (!directionOnly) for (const value of ticks(false)) {
            const y = py(value);
            ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
        }
        for (let time = first; time <= endTime; time += interval) {
            const x = px(time);
            ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke();
        }
        ctx.setLineDash([]);
        ctx.globalAlpha = plotStyle("borderOpacity");
        ctx.lineWidth = plotStyle("borderWidth");
        ctx.strokeRect(left, top, right - left, bottom - top);
        ctx.restore();
    }
    function toggle(key) {
        const field = plotFields.find(field => field.key === key);
        const group = field ? field.toggleKeys : [key], hidden = hiddenKeys.indexOf(key) < 0;
        hiddenKeys = hiddenKeys.filter(entry => group.indexOf(entry) < 0).concat(hidden ? group : []);
    }
    function tickLabel(value, right) {
        const axis = description[right ? "rightAxis" : "leftAxis"], index = axis.ticks.indexOf(value);
        return index >= 0 ? axis.labels[index] : Number(value).toLocaleString(Qt.locale(), 'f', 1);
    }
    function px(time) { return leftMargin + (time - startTime) / Math.max(1, endTime - startTime) * (plot.width - leftMargin - rightMargin); }
    function py(value, right) {
        const axis = description[right ? "rightAxis" : "leftAxis"];
        const levels = axis.levels;
        if (levels.length) {
            let index = 0;
            while (index < levels.length - 2 && value > levels[index + 1]) index++;
            const fraction = (index + Math.max(0, Math.min(1, (value - levels[index]) / (levels[index + 1] - levels[index])))) / (levels.length - 1);
            return plot.height - bottomMargin - fraction * (plot.height - topMargin - bottomMargin);
        }
        const b = axis.range;
        return topMargin + (b.high - value) / Math.max(0.0001, b.high - b.low) * (plot.height - topMargin - bottomMargin);
    }
    function ticks(right) { return description[right ? "rightAxis" : "leftAxis"].ticks; }
    function axisUnit(right) { return directionOnly || right && !hasRightAxis ? "" : description[right ? "rightAxis" : "leftAxis"].unit; }
    function axisMargin(right) {
        if (directionOnly) return 6;
        const axis = description[right ? "rightAxis" : "leftAxis"];
        let width = axisFont.advanceWidth(axisUnit(right));
        for (const label of axis.labels) width = Math.max(width, axisFont.advanceWidth(label));
        return Math.ceil(width) + (right ? 10 : 12);
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

    Component { id: saveDialogComponent; ChartSaveDialog {} }
    Menu {
        id: chartContextMenu
        objectName: "chartContextMenu"
        MenuItem {
            objectName: "saveChartMenuItem"
            text: I18n.i18n("Save chart as…")
            icon.name: "document-save-as"
            onTriggered: root.saveChart()
        }
        MenuSeparator {}
        Instantiator {
            model: root.legendFields
            delegate: MenuItem {
                required property var modelData
                objectName: "chartLayer-" + modelData.key
                text: root.legendLabel(modelData)
                checkable: true
                checked: root.shown(modelData)
                onTriggered: root.toggle(modelData.key)
            }
            onObjectAdded: (index, object) => chartContextMenu.insertItem(index + 2, object)
            onObjectRemoved: (index, object) => chartContextMenu.removeItem(object)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.showHeading
        Label { text: root.title; color: root.textColor; font.bold: true; Layout.maximumWidth: Math.max(0, parent.width - 40); elide: Text.ElideRight }
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
    ChartLegend {
        visible: root.legendVisible
        Layout.fillWidth: true
        chart: root
    }
    Item {
        id: plot
        objectName: "plot-" + root.groupKey
        Layout.fillWidth: true
        visible: !root.axisConflict
        Layout.preferredHeight: visible ? root.chartHeight + root.directionBandHeight : 0
        activeFocusOnTab: !root.exportMode
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Menu || event.key === Qt.Key_F10 && event.modifiers & Qt.ShiftModifier) {
                chartContextMenu.popup(plot, root.leftMargin, root.topMargin);
                event.accepted = true;
            }
        }
        property bool softwareFallback: false
        readonly property string activeRenderer: UserPreferences.renderer === "cpu" || GraphicsInfo.api === GraphicsInfo.Software || softwareFallback ? "cpu" : "gpu"
        // Own the MSAA texture on the regular container, not on a custom
        // scene-graph item; otherwise arrow-only items can leave an empty layer.
        layer.enabled: activeRenderer === "gpu"
        layer.samples: 4
        function requestPaint() {
            if (!root.renderEnabled && !root.exportMode) return;
            if (activeRenderer === "gpu") {
                const target = renderer.item as GpuPlot;
                if (target) target.requestPaint();
            } else {
                const target = renderer.item as Canvas;
                if (target) target.requestPaint();
            }
        }
        function requestCursorPaint() {
            if (!root.renderEnabled && !root.exportMode) return;
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
            visible: root.renderEnabled || root.exportMode
            sourceComponent: plot.activeRenderer === "gpu" ? gpuComponent : cpuComponent
            onLoaded: plot.requestPaint()
        }
        Component {
            id: gpuComponent
            GpuPlot {
                id: gpuPlot
                objectName: "gpu-base-" + root.groupKey
                onPaint: if (root.renderEnabled || root.exportMode) { Style.paintChart(root.renderDescription(), plot, getContext("2d"), false); gpuCursor.requestPaint(); }
                onCursorPaintRequested: gpuCursor.requestPaint()
                onSoftwareBackend: plot.softwareFallback = true
                PlotLabels {
                    anchors.fill: parent
                    labels: gpuPlot.textLabels
                    font: gpuPlot.labelFont
                }
                GpuPlot {
                    id: gpuCursor
                    objectName: "gpu-cursor-" + root.groupKey
                    anchors.fill: parent
                    onPaint: Style.paintCursor(root.renderDescription(), plot, getContext("2d"), true)
                }
            }
        }
        Component {
            id: cpuComponent
            Canvas {
                objectName: "cpu-base-" + root.groupKey
                property int renderedFrames: 0
                renderTarget: Canvas.Image
                onPaint: if (root.renderEnabled || root.exportMode) { Style.paintChart(root.renderDescription(), plot, getContext("2d")); ++renderedFrames; }
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: !root.exportMode
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: root.identificationEnabled
            cursorShape: root.identificationEnabled ? Qt.CrossCursor : Qt.ArrowCursor
            onPositionChanged: mouse => {
                if (!root.identificationEnabled || chartContextMenu.visible || gestures.active) return;
                if (mouse.x < root.leftMargin || mouse.x > width - root.rightMargin) return;
                const time = root.startTime + (mouse.x - root.leftMargin) / Math.max(1, width - root.leftMargin - root.rightMargin) * (root.endTime - root.startTime);
                root.probeMoved(root.weather.nearestTimestamp(time), mapToItem(null, mouse.x, mouse.y));
            }
            onExited: root.probeLeft()
            onClicked: mouse => {
                if (mouse.button === Qt.RightButton) {
                    plot.forceActiveFocus();
                    chartContextMenu.popup(plot, mouse.x, mouse.y);
                    return;
                }
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
        ChartGestures {
            id: gestures
            enabled: root.touchNavigationEnabled && !root.exportMode
            x: root.leftMargin
            y: root.topMargin
            width: Math.max(1, plot.width - root.leftMargin - root.rightMargin)
            height: Math.max(1, plot.height - root.topMargin - root.bottomMargin)
            startTime: root.startTime
            endTime: root.endTime
            maximumSpan: Math.max(0, root.weather.lastTimestamp - root.weather.firstTimestamp)
            minimumSpan: Math.min(24 * 3600, maximumSpan)
            onRangeRequested: (first, last) => root.rangeRequested(first, last)
            onResetRequested: root.resetRangeRequested()
        }
    }
    Kirigami.InlineMessage {
        objectName: "chartAxisConflict"
        Layout.fillWidth: true
        visible: root.axisConflict
        text: root.chartConfiguration.axisConflict || ""
        type: Kirigami.MessageType.Warning
    }
    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: !root.exportMode && text.length > 0
        text: root.saveDialog ? root.saveDialog.error : ""
        type: Kirigami.MessageType.Error
        showCloseButton: true
        onVisibleChanged: if (!visible && root.saveDialog) root.saveDialog.error = ""
    }
    Label {
        visible: root.legendVisible && root.plotFields.some(f => f.key === "pcpttl_aver" || f.key === "pcpttl_max")
        Layout.fillWidth: true
        text: ChartAnalysis.precipitationCaption(root.fields)
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
        function onLeftMarginChanged() { plot.requestPaint(); }
        function onRightMarginChanged() { plot.requestPaint(); }
        function onSelectedTimeChanged() { plot.requestCursorPaint(); }
        function onExportModeChanged() { plot.requestPaint(); }
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
