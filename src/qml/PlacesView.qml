pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "placesView"
    property WeatherModel weather: null
    property var removed: null
    property string error: ""
    signal addRequested()
    signal editRequested(var place)
    signal locationSelected(string name, real latitude, real longitude)
    spacing: Kirigami.Units.largeSpacing
    function remove(place) {
        const result = UserPreferences.deletePlace(place.id);
        if (result.ok) { removed = result; error = ""; }
        else error = result.error;
        notice.visible = true;
    }
    function undoRemoval() {
        if (!removed) return;
        const result = UserPreferences.restorePlace(removed.place, removed.index);
        if (result.ok) { removed = null; error = ""; notice.visible = false; }
        else error = result.error;
    }
    Kirigami.InlineMessage {
        id: notice
        Layout.fillWidth: true
        visible: false
        type: root.error.length ? Kirigami.MessageType.Error : Kirigami.MessageType.Information
        text: root.error || (root.removed ? I18n.i18n("Removed: %1").arg(root.removed.place.name) : "")
        showCloseButton: true
        onVisibleChanged: if (!visible) { root.removed = null; root.error = ""; }
        actions: [Kirigami.Action {
            text: I18n.i18n("Undo")
            icon.name: "edit-undo"
            visible: root.removed !== null
            onTriggered: root.undoRemoval()
        }]
    }
    Pane {
        Layout.fillWidth: true
        padding: 0
        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        contentItem: ColumnLayout {
            spacing: 0
            Kirigami.InlineViewHeader {
                Layout.fillWidth: true
                text: I18n.i18n("My places")
                actions: [Kirigami.Action {
                    text: I18n.i18n("Add location")
                    icon.name: "list-add"
                    displayComponent: ToolButton {
                        objectName: "addPlaceButton"
                        text: I18n.i18n("Add location")
                        icon.name: "list-add"
                        onClicked: root.addRequested()
                    }
                }]
            }
            Repeater {
                model: UserPreferences.places
                delegate: ItemDelegate {
                    id: row
                    required property var modelData
                    objectName: "savedPlace-" + modelData.id
                    Layout.fillWidth: true
                    highlighted: root.weather && LocationRules.samePlace(modelData.latitude, modelData.longitude, root.weather.latitude, root.weather.longitude)
                    Accessible.name: I18n.i18n("Show forecast: %1").arg(modelData.name)
                    onClicked: root.locationSelected(modelData.name, modelData.latitude, modelData.longitude)
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing
                        Kirigami.Icon { source: "mark-location"; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
                        Label { text: row.modelData.name; Layout.fillWidth: true; Layout.minimumWidth: 0; wrapMode: Text.WordWrap }
                        ToolButton {
                            objectName: "editPlace-" + row.modelData.id
                            icon.name: "document-edit"
                            text: I18n.i18n("Edit place")
                            display: AbstractButton.IconOnly
                            ToolTip.text: text; ToolTip.visible: hovered
                            onClicked: root.editRequested(row.modelData)
                        }
                        ToolButton {
                            objectName: "deletePlace-" + row.modelData.id
                            icon.name: "edit-delete"
                            text: I18n.i18n("Remove place: ") + row.modelData.name
                            display: AbstractButton.IconOnly
                            ToolTip.text: text; ToolTip.visible: hovered
                            onClicked: root.remove(row.modelData)
                        }
                    }
                }
            }
            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing * 2
                visible: UserPreferences.places.length === 0
                text: I18n.i18n("No saved places")
                explanation: I18n.i18n("Add a place to see its weather forecast.")
            }
        }
    }
}
