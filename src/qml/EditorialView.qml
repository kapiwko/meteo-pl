pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property bool commentary: true
    property bool initialized: false
    property var selectedArticle: null
    readonly property string today: Qt.formatDate(new Date(), "yyyy-MM-dd")
    readonly property string shownDate: feed.date || feed.publishedDate.slice(0, 10) || today
    EditorialModel { id: feed; commentary: root.commentary }
    Component.onCompleted: { initialized = true; feed.reload(); }
    onCommentaryChanged: if (initialized) { selectedArticle = null; Qt.callLater(feed.reload); }
    function shiftDate(delta) {
        const date = new Date(shownDate + "T12:00:00");
        date.setDate(date.getDate() + delta);
        feed.date = Qt.formatDate(date, "yyyy-MM-dd");
    }
    RowLayout {
        Layout.fillWidth: true
        Kirigami.Heading { text: root.commentary ? "Komentarz synoptyka" : root.selectedArticle ? "Artykuł" : "Artykuły meteo.pl"; level: 2; Layout.fillWidth: true }
        ToolButton { text: "Odśwież"; icon.name: "view-refresh"; enabled: !feed.busy; onClicked: feed.reload(); ToolTip.text: text; ToolTip.visible: hovered }
    }
    RowLayout {
        visible: root.commentary
        Layout.fillWidth: true
        Button { text: "‹"; enabled: !feed.busy; onClicked: root.shiftDate(-1) }
        TextField {
            id: dateField
            Layout.fillWidth: true
            Binding on text { value: root.shownDate; when: !dateField.activeFocus; restoreMode: Binding.RestoreNone }
            placeholderText: "rrrr-mm-dd"
            inputMask: "9999-99-99"
            onAccepted: feed.date = text
        }
        Button { text: "›"; enabled: !feed.busy && root.shownDate < root.today; onClicked: root.shiftDate(1) }
        Button { text: "Najnowszy"; enabled: !feed.busy; onClicked: { if (feed.date.length) feed.date = ""; else feed.reload(); } }
    }
    Button { visible: !root.commentary && root.selectedArticle !== null; text: "Wróć do artykułów"; icon.name: "go-previous"; onClicked: root.selectedArticle = null }
    BusyIndicator { running: feed.busy; visible: running; Layout.alignment: Qt.AlignHCenter }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: feed.error.length > 0; text: feed.error; type: Kirigami.MessageType.Warning }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: feed.notice.length > 0; text: feed.notice; type: Kirigami.MessageType.Information }
    ScrollView {
        id: readerScroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: readerScroll.availableWidth
            spacing: 16
            Label { visible: root.selectedArticle !== null; text: root.selectedArticle ? root.selectedArticle.title : ""; font.bold: true; font.pixelSize: 20; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TextArea {
                visible: root.commentary || root.selectedArticle !== null
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WordWrap
                textFormat: TextEdit.RichText
                text: root.commentary ? feed.commentHtml : root.selectedArticle ? root.selectedArticle.html : ""
                font.pixelSize: 15
                background: null
                onLinkActivated: link => { if (link.startsWith("https://") || link.startsWith("http://")) Qt.openUrlExternally(link); }
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
                        Image { source: article.modelData.image; asynchronous: true; fillMode: Image.PreserveAspectCrop; Layout.preferredWidth: 100; Layout.preferredHeight: 70; visible: source.toString().length > 0 }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: article.modelData.title; textFormat: Text.RichText; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Label { text: article.modelData.date; opacity: 0.65 }
                        }
                    }
                }
            }
            Button { visible: !root.commentary && !root.selectedArticle; text: "Wczytaj starsze artykuły"; enabled: !feed.busy; onClicked: feed.more() }
            Button { visible: root.selectedArticle !== null; text: "Czytaj w meteo.pl"; icon.name: "internet-web-browser"; onClicked: if (root.selectedArticle) Qt.openUrlExternally(root.selectedArticle.url) }
            Label { text: "Źródło: ICM UW / meteo.pl"; opacity: 0.65; font.pixelSize: 11 }
        }
    }
}
