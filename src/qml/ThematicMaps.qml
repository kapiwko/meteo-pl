pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property int layerIndex: 0
    property int step: 12
    property real run: 0
    property real imageScale: 1
    property int attempts: 0
    readonly property var catalog: [
        {name: "UM4 · ciśnienie", id: "SLPH_0000.0_0U", model: "um"},
        {name: "UM4 · temperatura i linie prądu", id: "T+WH_0000.0_0U", model: "um"},
        {name: "UM4 · temperatura powierzchni", id: "GT_H_0000.0_0U", model: "um"},
        {name: "UM4 · opady", id: "RS_H_0000.0_0U", model: "um"},
        {name: "UM4 · burze", id: "FLSH_0000.0_0U", model: "um"},
        {name: "UM4 · wiatr przy powierzchni", id: "W__H_0000.0_0U", model: "um"},
        {name: "UM4 · maksymalne porywy", id: "WGSH_0000.0_0U", model: "um"},
        {name: "UM4 · widzialność", id: "VISH_0000.0_0U", model: "um"},
        {name: "UM4 · mgła", id: "F__H_0000.0_0U", model: "um"},
        {name: "UM4 · wilgotność względem lodu", id: "RHiH_0000.0_0U", model: "um"},
        {name: "UM4 · wilgotność względem wody", id: "RHwH_0000.0_0U", model: "um"},
        {name: "UM4 · chmury bardzo niskie", id: "CV_H_0000.0_0U", model: "um"},
        {name: "UM4 · chmury niskie", id: "CL_H_0000.0_0U", model: "um"},
        {name: "UM4 · chmury średnie", id: "CM_H_0000.0_0U", model: "um"},
        {name: "UM4 · chmury wysokie", id: "CH_H_0000.0_0U", model: "um"},
        {name: "UM4 · zachmurzenie całkowite", id: "CT_H_0000.0_0U", model: "um"},
        {name: "WAM · wysokość fali znacznej", id: "wavehgt_0W", model: "wamcoamps"},
        {name: "WAM · średni okres fali", id: "m_period_0W", model: "wamcoamps"},
        {name: "WAM · okres szczytowy fali", id: "p_period_0W", model: "wamcoamps"}
    ]
    readonly property bool wave: catalog[layerIndex].model !== "um"
    readonly property int intervalHours: wave ? 3 : 1
    readonly property string runKey: Qt.formatDateTime(new Date(run * 1000), "yyyyMMddhh")
    readonly property string imageUrl: urlForStep(step)
    readonly property string requestedLabel: catalog[layerIndex].name + " · start: " + weather.formatTime(run, "ddd d MMM, HH:mm t") + " · +" + step + " h"
    readonly property string animationKey: layerIndex + ":" + run
    readonly property real fitScale: picture.implicitWidth > 0 && picture.implicitHeight > 0 ? Math.min(imageScroll.availableWidth / picture.implicitWidth, imageScroll.availableHeight / picture.implicitHeight) : 1
    onAnimationKeyChanged: { animationCache.cancel(); if (animate.checked) prepareAnimation(); }
    function prepareAnimation() {
        if (!animate.checked || !run || picture.loading || !picture.displayedSource.toString().length || animationCache.busy || animationCache.ready) return;
        const urls = [];
        for (let hour = intervalHours; hour <= (wave ? 72 : 60); hour += intervalHours) urls.push(urlForStep(hour));
        animationCache.start(urls);
    }
    ImagePreloader {
        id: animationCache
        onFinished: MapImages.prefetch(root.urlForStep(root.nextStep()))
        onFailed: animate.checked = false
    }
    function urlForStep(hour) {
        if (!run) return "";
        const date = new Date(run * 1000);
        const stamp = String(date.getUTCFullYear()) + String(date.getUTCMonth()+1).padStart(2,"0") + String(date.getUTCDate()).padStart(2,"0") + String(date.getUTCHours()).padStart(2,"0");
        return "https://old.meteo.pl/" + catalog[layerIndex].model + "/pict/" + stamp + "/" + encodeURIComponent(catalog[layerIndex].id) + "_" + stamp + "_" + String(hour).padStart(3,"0") + "-00.png";
    }
    function nextStep() { return step >= (wave ? 72 : 60) ? intervalHours : step + intervalHours; }
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
    function latest() { attempts = 0; run = Math.floor(Date.now() / 1000 / ((wave ? 12 : 6) * 3600)) * ((wave ? 12 : 6) * 3600); }
    Component.onCompleted: latest()
    onWaveChanged: { step = wave ? 12 : 12; latest(); }
    ComboBox { Layout.fillWidth: true; model: root.catalog; textRole: "name"; currentIndex: root.layerIndex; onActivated: index => { animate.checked = false; root.layerIndex = index; root.attempts = 0; root.imageScale = 1; } }
    RowLayout {
        Layout.fillWidth: true
        Button { text: "‹"; onClicked: { root.run -= (root.wave ? 12 : 6) * 3600; root.attempts = 4; } }
        Label { Layout.fillWidth: true; text: picture.displayedLabel || root.requestedLabel; wrapMode: Text.WordWrap }
        Button { text: "Najnowsza"; onClicked: root.latest() }
        Button { text: "›"; enabled: root.run + (root.wave ? 12 : 6) * 3600 < Date.now() / 1000; onClicked: { root.run += (root.wave ? 12 : 6) * 3600; root.attempts = 4; } }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { id: animate; text: checked ? animationCache.ready ? "Pauza" : "Anuluj ładowanie" : "Animuj"; checkable: true; enabled: root.run > 0 && picture.displayedSource.toString().length > 0; onCheckedChanged: { if (checked) root.prepareAnimation(); else animationCache.cancel(); } }
        Slider { Layout.fillWidth: true; from: root.intervalHours; to: root.wave ? 72 : 60; stepSize: root.intervalHours; value: root.step; onMoved: root.step = Math.round(value) }
        ToolButton { icon.name: "zoom-in"; onClicked: root.zoomImage(1.25, imageScroll.availableWidth / 2, imageScroll.availableHeight / 2) }
        ToolButton { icon.name: "zoom-out"; onClicked: root.zoomImage(0.8, imageScroll.availableWidth / 2, imageScroll.availableHeight / 2) }
        ToolButton { icon.name: "zoom-fit-best"; text: "Dopasuj"; display: AbstractButton.IconOnly; onClicked: root.imageScale = 1; ToolTip.text: "Dopasuj całą mapę do widoku"; ToolTip.visible: hovered }
    }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: picture.error.length > 0 && root.attempts >= 4; type: Kirigami.MessageType.Warning; text: "Mapa dla tego uruchomienia i kroku nie jest dostępna. Wybierz wcześniejszy start lub inną godzinę." }
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
                onFrameCommitted: if (animate.checked) { root.prepareAnimation(); if (animationCache.ready) MapImages.prefetch(root.urlForStep(root.nextStep())); }
                onFailed: {
                    if (root.attempts < 4) {
                        root.attempts++;
                        root.run -= (root.wave ? 12 : 6) * 3600;
                    } else animate.checked = false;
                }
            }
        }
        Column {
            anchors.centerIn: parent
            spacing: 8
            BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: picture.loading || animationCache.busy; visible: running }
            Label { visible: animationCache.busy; text: "Przygotowywanie animacji: " + animationCache.completed + " / " + animationCache.total; padding: 8; background: Rectangle { radius: 4; color: Kirigami.Theme.backgroundColor } }
            ProgressBar { anchors.horizontalCenter: parent.horizontalCenter; width: 200; visible: animationCache.busy; from: 0; to: Math.max(1, animationCache.total); value: animationCache.completed }
        }
        Item {
            anchors.fill: parent
            MapWheelHandler { onZoomRequested: (steps, position) => root.zoomImage(Math.pow(1.25, steps), position.x, position.y) }
        }
    }
    Label { text: "Mapy tematyczne ICM UW / meteo.pl · wskazany start i krok prognozy"; font.pixelSize: 10; opacity: 0.7; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    Timer { interval: 1300; repeat: true; running: animate.checked && animationCache.ready && !picture.loading && !picture.error.length && picture.displayedSource.toString().length > 0; onTriggered: root.step = root.nextStep() }
}
