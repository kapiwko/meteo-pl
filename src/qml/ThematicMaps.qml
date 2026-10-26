pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property alias layerIndex: thematic.layerIndex
    property alias step: thematic.step
    property alias run: thematic.run
    property real imageScale: 1
    readonly property var catalog: thematic.catalog
    readonly property bool wave: thematic.wave
    readonly property int intervalHours: thematic.stepHours
    readonly property url imageUrl: thematic.imageUrl
    ThematicMapsModel { id: thematic; objectName: "thematicMapsModel" }
    Connections {
        target: MapImages
        function onImageFailed(url, message, status) { thematic.imageFailed(url,message,status); }
    }
    Connections {
        target: thematic
        function onChanged() { if (thematic.error.length) animate.checked=false; }
        function onReloadRequested() { picture.reload(); }
    }
    readonly property string requestedLabel: labelForStep(step)
    function labelForStep(hour) { return catalog[layerIndex].name + I18n.i18n(" · run: ") + weather.formatTime(run, "ddd d MMM, HH:mm t") + " · +" + hour + " h"; }
    readonly property string animationKey: layerIndex + ":" + run
    readonly property real fitScale: picture.implicitWidth > 0 && picture.implicitHeight > 0 ? Math.min(imageScroll.availableWidth / picture.implicitWidth, imageScroll.availableHeight / picture.implicitHeight) : 1
    onAnimationKeyChanged: { animationCache.cancel(); if (animate.checked) prepareAnimation(); }
    function prepareAnimation() {
        if (!animate.checked || !run || picture.loading || !picture.displayedSource.toString().length || animationCache.busy || animationCache.ready) return;
        animationCache.start(thematic.animationUrls());
    }
    ImagePreloader {
        id: animationCache
        onFinished: picture.prepare(root.urlForStep(root.nextStep()), root.labelForStep(root.nextStep()))
        onFailed: animate.checked = false
    }
    function urlForStep(hour) { return thematic.urlForStep(hour).toString(); }
    function nextStep() { return thematic.nextStep(); }
    function zoomImage(factor, x, y) {
        const flickable = imageScroll.contentItem as Flickable;
        if (!flickable) return;
        const horizontal = (flickable.contentX + x - picture.x) / Math.max(1, picture.width);
        const vertical = (flickable.contentY + y - picture.y) / Math.max(1, picture.height);
        imageScale = Math.max(0.25, Math.min(8, imageScale * factor));
        Qt.callLater(() => {
            flickable.contentX = Math.max(0, Math.min(picture.width - imageScroll.availableWidth, horizontal * picture.width + picture.x - x));
            flickable.contentY = Math.max(0, Math.min(picture.height - imageScroll.availableHeight, vertical * picture.height + picture.y - y));
        });
    }
    function latest() { thematic.latest(); }
    ComboBox { Layout.fillWidth: true; model: root.catalog; textRole: "name"; currentIndex: root.layerIndex; onActivated: index => { animate.checked = false; root.layerIndex = index; root.imageScale = 1; } }
    RowLayout {
        Layout.fillWidth: true
        ToolButton { icon.name: "go-previous"; Accessible.name: I18n.i18n("Earlier"); onClicked: thematic.earlier() }
        Label { Layout.fillWidth: true; text: picture.displayedLabel || root.requestedLabel; wrapMode: Text.WordWrap }
        Button { text: I18n.i18n("Latest"); onClicked: root.latest() }
        ToolButton { icon.name: "go-next"; Accessible.name: I18n.i18n("Later"); enabled: thematic.canGoLater; onClicked: thematic.later() }
    }
    GridLayout {
        id: animationControls
        Layout.fillWidth: true
        columns: width < 500 ? 3 : 5
        Button { id: animate; text: checked ? animationCache.ready ? I18n.i18n("Pause") : I18n.i18n("Cancel loading") : I18n.i18n("Animate"); checkable: true; enabled: root.run > 0 && picture.displayedSource.toString().length > 0; onCheckedChanged: { if (checked) root.prepareAnimation(); else animationCache.cancel(); } }
        Slider { Layout.fillWidth: true; Layout.columnSpan: animationControls.columns === 3 ? 2 : 1; Layout.minimumWidth: 60; from: root.intervalHours; to: thematic.lastStep; stepSize: root.intervalHours; value: root.step; onMoved: root.step = Math.round(value) }
        ToolButton { icon.name: "zoom-in"; onClicked: root.zoomImage(1.25, imageScroll.availableWidth / 2, imageScroll.availableHeight / 2) }
        ToolButton { icon.name: "zoom-out"; onClicked: root.zoomImage(0.8, imageScroll.availableWidth / 2, imageScroll.availableHeight / 2) }
        ToolButton { icon.name: "zoom-fit-best"; text: I18n.i18n("Fit"); display: AbstractButton.IconOnly; onClicked: root.imageScale = 1; ToolTip.text: I18n.i18n("Fit the entire map to the view"); ToolTip.visible: hovered }
    }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: thematic.error.length > 0; type: Kirigami.MessageType.Warning; text: thematic.error }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: animationCache.error.length > 0; type: Kirigami.MessageType.Warning; text: animationCache.error }
    Item {
        objectName: "thematicViewport"
        Layout.fillWidth: true
        Layout.fillHeight: true
        ScrollView {
            id: imageScroll
            anchors.fill: parent
            clip: true
            contentWidth: Math.max(availableWidth, picture.width)
            contentHeight: Math.max(availableHeight, picture.height)
            BufferedImage {
                id: picture
                objectName: "thematicPicture"
                source: root.imageUrl
                label: root.requestedLabel
                x: Math.max(0, (imageScroll.availableWidth - width) / 2)
                y: Math.max(0, (imageScroll.availableHeight - height) / 2)
                width: implicitWidth > 0 ? implicitWidth * root.fitScale * root.imageScale : imageScroll.availableWidth
                height: implicitHeight > 0 ? implicitHeight * root.fitScale * root.imageScale : imageScroll.availableHeight
                onFrameCommitted: {
                    thematic.imageReady(displayedSource);
                    if (animate.checked) { root.prepareAnimation(); if (animationCache.ready) picture.prepare(root.urlForStep(root.nextStep()), root.labelForStep(root.nextStep())); }
                }
            }
        }
        Column {
            anchors.centerIn: parent
            spacing: 8
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: picture.loading || animationCache.busy; visible: running }
            Label { visible: animationCache.busy; text: I18n.i18n("Preparing animation: ") + animationCache.completed + " / " + animationCache.total; padding: 8; background: Rectangle { radius: 4; color: Kirigami.Theme.backgroundColor } }
            ProgressBar { anchors.horizontalCenter: parent.horizontalCenter; width: 200; visible: animationCache.busy; from: 0; to: Math.max(1, animationCache.total); value: animationCache.completed }
        }
        Item {
            anchors.fill: parent
            MapWheelHandler { onZoomRequested: (steps, position) => root.zoomImage(Math.pow(1.25, steps), position.x, position.y) }
        }
    }
    Label { text: I18n.i18n("ICM UW / meteo.pl thematic maps · selected run and forecast step"); font.pixelSize: 10; opacity: 0.7; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    Timer { interval: 500; repeat: true; running: animate.checked && animationCache.ready && !picture.error.length && picture.displayedSource.toString().length > 0; onTriggered: if (!picture.loading) root.step = root.nextStep() }
}
