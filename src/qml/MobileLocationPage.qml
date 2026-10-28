pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "mobileLocationPage"
    required property var navigationStack
    property var place: ({})
    property bool firstRun: false
    property string saveError: ""
    readonly property bool editing: !!place.id
    signal placeSaved(var place, bool added)
    title: firstRun ? I18n.i18n("Welcome to Meteo") : editing ? I18n.i18n("Edit place") : I18n.i18n("Add location")
    padding: Kirigami.Units.largeSpacing
    header: MobilePageHeader { title: root.title; onBackRequested: root.navigationStack.pop() }
    ColumnLayout {
        width: root.availableWidth
        Label {
            Layout.fillWidth: true
            visible: root.firstRun
            text: I18n.i18n("Add your first place to get started. Search by name, choose a point on the map or use your location.")
            wrapMode: Text.WordWrap
        }
        LocationEditor {
            id: editor
            Layout.fillWidth: true
            editingId: root.place.id || ""
            Component.onCompleted: resetDraft(root.editing ? root.place.name : "", root.place.latitude, root.place.longitude)
        }
        Kirigami.InlineMessage { Layout.fillWidth: true; visible: root.saveError.length > 0; text: root.saveError; type: Kirigami.MessageType.Error }
    }
    footer: Pane {
        padding: Kirigami.Units.largeSpacing
        contentItem: Button {
            objectName: "mobileSavePlaceButton"
            text: root.editing ? I18n.i18n("Save") : I18n.i18n("Add")
            icon.name: root.editing ? "document-save" : "list-add"
            highlighted: true
            enabled: editor.valid && !root.navigationStack.busy
            onClicked: {
                const result = UserPreferences.editPlace(root.place.id || "", editor.locationName, editor.latitude, editor.longitude);
                if (result.ok) root.placeSaved(result.place, !root.editing);
                else root.saveError = result.error;
            }
        }
    }
}
