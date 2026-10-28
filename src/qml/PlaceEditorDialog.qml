pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "locationDialog"
    property var place: ({})
    property bool firstRun: false
    property string saveError: ""
    readonly property bool editing: !!place.id
    signal placeSaved(var place, bool added)
    title: firstRun ? I18n.i18n("Welcome to Meteo") : editing ? I18n.i18n("Edit place") : I18n.i18n("Add location")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, parent ? parent.width - 24 : 640)
    height: Math.min(implicitHeight, parent ? parent.height - 24 : 760, 760)
    padding: 0
    modal: true
    focus: true
    onAboutToShow: {
        saveError = "";
        editor.editingId = place.id || "";
        editor.resetDraft(editing ? place.name : "", place.latitude, place.longitude);
    }
    function save() {
        if (!editor.valid) return;
        const result = UserPreferences.editPlace(place.id || "", editor.locationName, editor.latitude, editor.longitude);
        if (!result.ok) { saveError = result.error; return; }
        placeSaved(result.place, !editing);
        accept();
    }
    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Cancel
        Button {
            objectName: "saveLocationButton"
            text: root.editing ? I18n.i18n("Save") : I18n.i18n("Add")
            icon.name: root.editing ? "document-save" : "list-add"
            enabled: editor.valid
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: root.save()
        }
    }
    contentItem: ScrollView {
        id: scroll
        implicitWidth: 0
        implicitHeight: content.implicitHeight
        contentWidth: availableWidth
        clip: true
        focus: true
        Keys.onShortcutOverride: event => { if (event.key === Qt.Key_Back) event.accepted = true; }
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Back) {
                event.accepted = true;
                if (Qt.inputMethod.visible || Qt.inputMethod.animating) Qt.inputMethod.hide();
                else root.reject();
            }
        }
        Pane {
            id: content
            objectName: "locationDialogContent"
            width: scroll.availableWidth
            padding: Kirigami.Units.largeSpacing * 2
            background: null
            contentItem: Column {
                spacing: Kirigami.Units.largeSpacing
                Label {
                    width: parent.width
                    visible: root.firstRun
                    text: I18n.i18n("Add your first place to get started. Search by name, choose a point on the map or use your location.")
                    wrapMode: Text.WordWrap
                }
                LocationEditor { id: editor; width: parent.width }
                Kirigami.InlineMessage { width: parent.width; visible: root.saveError.length > 0; text: root.saveError; type: Kirigami.MessageType.Error }
            }
        }
    }
}
