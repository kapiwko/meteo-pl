pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import pl.meteo.desktop

KCM.SimpleKCM {
    id: root
    property var cfg_visiblePlaces: ["*"]
    property var cfg_placeOrder: []
    readonly property bool allPlaces: cfg_visiblePlaces.indexOf("*") >= 0
    readonly property bool isSaveAllowed: catalog.places.length === 0 || allPlaces
        || catalog.places.some(place => catalog.includesPlace(cfg_visiblePlaces, place.latitude, place.longitude))
    SavedPlaceSelection { id: catalog; weather: null; orderKeys: root.cfg_placeOrder }
    function move(index, direction) {
        const keys = catalog.places.map(place => catalog.placeKey(place.latitude, place.longitude));
        const next = index + direction;
        if (next < 0 || next >= keys.length) return;
        const previous = keys[index]; keys[index] = keys[next]; keys[next] = previous;
        cfg_placeOrder = keys;
    }
    Kirigami.FormLayout {
        CheckBox {
            text: I18n.i18n("Show all saved places, including new ones")
            checked: root.allPlaces
            onToggled: root.cfg_visiblePlaces = checked ? ["*"]
                : catalog.places.map(place => catalog.placeKey(place.latitude, place.longitude))
        }
        ColumnLayout {
            Kirigami.FormData.label: I18n.i18n("Places:")
            Layout.fillWidth: true
            Repeater {
                model: catalog.places
                delegate: RowLayout {
                    id: row
                    required property int index
                    required property var modelData
                    readonly property string key: catalog.placeKey(modelData.latitude, modelData.longitude)
                    Layout.fillWidth: true
                    CheckBox {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        enabled: !root.allPlaces
                        checked: root.allPlaces || catalog.includesPlace(root.cfg_visiblePlaces, row.modelData.latitude, row.modelData.longitude)
                        onToggled: {
                            const legacy = row.modelData.latitude.toFixed(6) + "," + row.modelData.longitude.toFixed(6);
                            const keys = Array.from(root.cfg_visiblePlaces).filter(key => key !== row.key && key !== legacy);
                            if (checked) keys.push(row.key);
                            root.cfg_visiblePlaces = keys;
                        }
                    }
                    ToolButton {
                        icon.name: "go-up-symbolic"
                        text: I18n.i18n("Move up")
                        display: AbstractButton.IconOnly
                        enabled: row.index > 0
                        onClicked: root.move(row.index, -1)
                        ToolTip.text: text
                        ToolTip.visible: hovered
                    }
                    ToolButton {
                        icon.name: "go-down-symbolic"
                        text: I18n.i18n("Move down")
                        display: AbstractButton.IconOnly
                        enabled: row.index + 1 < catalog.places.length
                        onClicked: root.move(row.index, 1)
                        ToolTip.text: text
                        ToolTip.visible: hovered
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: catalog.places.length ? I18n.i18n("Choose the places and their order for this widget. Manage saved places on the Location page.")
                    : I18n.i18n("Add a place on the Location page first.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: !root.isSaveAllowed
                text: I18n.i18n("Select at least one place.")
                type: Kirigami.MessageType.Information
            }
        }
    }
}
