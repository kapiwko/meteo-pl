pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property bool commentary: true
    property bool mobileLayout: false
    property bool initialized: false
    property var selectedArticle: null
    readonly property string today: feed.today
    readonly property string shownDate: feed.date || feed.publishedDate.slice(0, 10) || today
    EditorialModel { id: feed; commentary: root.commentary }
    Component.onCompleted: { initialized = true; feed.reload(false); }
    onCommentaryChanged: if (initialized) { selectedArticle = null; Qt.callLater(() => feed.reload(false)); }
    function shiftDate(delta) {
        if (feed.busy || delta > 0 && shownDate >= today) return;
        const date = new Date(shownDate + "T12:00:00");
        date.setDate(date.getDate() + delta);
        feed.date = Qt.formatDate(date, "yyyy-MM-dd");
    }
    RowLayout {
        Layout.fillWidth: true
        Kirigami.Heading { text: root.commentary ? I18n.i18n("Synoptic commentary") : root.selectedArticle ? I18n.i18n("Article") : I18n.i18n("meteo.pl articles"); level: 2; Layout.fillWidth: true }
        ToolButton { text: I18n.i18n("Refresh"); icon.name: "view-refresh"; enabled: !feed.busy; onClicked: feed.reload(); ToolTip.text: text; ToolTip.visible: hovered }
    }
    RowLayout {
        visible: root.commentary
        Layout.fillWidth: true
        Button { objectName: "synopticPreviousDay"; visible: !root.mobileLayout; text: "‹"; enabled: !feed.busy; onClicked: root.shiftDate(-1) }
        TextField {
            id: dateField
            Layout.fillWidth: true
            Binding on text { value: root.shownDate; when: !dateField.activeFocus; restoreMode: Binding.RestoreNone }
            placeholderText: I18n.i18n("yyyy-mm-dd")
            inputMask: "9999-99-99"
            onAccepted: feed.date = text
        }
        Button { objectName: "synopticNextDay"; visible: !root.mobileLayout; text: "›"; enabled: !feed.busy && root.shownDate < root.today; onClicked: root.shiftDate(1) }
        Button { text: I18n.i18n("Latest"); enabled: !feed.busy; onClicked: { if (feed.date.length) feed.date = ""; else feed.reload(); } }
    }
    Label {
        visible: root.commentary && root.mobileLayout
        Layout.fillWidth: true
        text: I18n.i18n("Swipe left or right to change the day")
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        horizontalAlignment: Text.AlignHCenter
    }
    Button { visible: !root.commentary && root.selectedArticle !== null; text: I18n.i18n("Back to articles"); icon.name: "go-previous"; onClicked: root.selectedArticle = null }
    BusyIndicator { running: feed.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: feed.error.length > 0; text: feed.error; type: Kirigami.MessageType.Warning }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: feed.notice.length > 0; text: feed.notice; type: Kirigami.MessageType.Information }
    ScrollView {
        id: readerScroll
        objectName: "editorialReader"
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        DragHandler {
            target: null
            enabled: root.commentary && root.mobileLayout && !feed.busy
            acceptedDevices: PointerDevice.TouchScreen
            maximumPointCount: 1
            yAxis.enabled: false
            property real distance: 0
            onActiveTranslationChanged: if (active) distance = activeTranslation.x
            onActiveChanged: {
                if (active) distance = 0;
                else if (enabled && root.visible && Math.abs(distance) >= Math.min(80, readerScroll.width * 0.2))
                    root.shiftDate(distance < 0 ? 1 : -1);
            }
        }
        ColumnLayout {
            width: readerScroll.availableWidth
            spacing: 16
            Label { visible: root.selectedArticle !== null; text: root.selectedArticle ? root.selectedArticle.title : ""; font.bold: true; font.pixelSize: 20; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TextArea {
                id: articleBody
                visible: root.commentary || root.selectedArticle !== null
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                clip: true
                textFormat: TextEdit.RichText
                text: MapImages.cachedHtml(root.commentary ? feed.commentHtml : root.selectedArticle ? root.selectedArticle.html : "", root.selectedArticle && root.selectedArticle.url ? root.selectedArticle.url : "https://www.meteo.pl/")
                font.pixelSize: 15
                background: null
                ResponsiveImages { document: articleBody.textDocument; maximumWidth: articleBody.width - articleBody.leftPadding - articleBody.rightPadding }
                onLinkActivated: link => WebLinks.open(link, root.selectedArticle && root.selectedArticle.url ? root.selectedArticle.url : "https://www.meteo.pl/")
            }
            Repeater {
                model: !root.commentary && !root.selectedArticle ? feed.articles : []
                delegate: ItemDelegate {
                    id: article
                    required property var modelData
                    Layout.fillWidth: true
                    onClicked: root.selectedArticle = modelData
                    contentItem: RowLayout {
                        spacing: 16
                        Image { source: MapImages.cached(article.modelData.image); cache: false; asynchronous: true; fillMode: Image.PreserveAspectCrop; Layout.preferredWidth: 100; Layout.preferredHeight: 70; visible: source.toString().length > 0 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: article.modelData.title; textFormat: Text.RichText; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Label { text: article.modelData.date; opacity: 0.65 }
                        }
                    }
                }
            }
            Button { visible: !root.commentary && !root.selectedArticle; text: I18n.i18n("Load older articles"); enabled: !feed.busy; onClicked: feed.more() }
            Button { visible: root.selectedArticle !== null; text: I18n.i18n("Read on meteo.pl"); icon.name: "internet-web-browser"; onClicked: if (root.selectedArticle) WebLinks.open(root.selectedArticle.url) }
            Label { text: I18n.i18n("Source: ICM UW / meteo.pl"); opacity: 0.65; font.pixelSize: 11 }
        }
    }
}
