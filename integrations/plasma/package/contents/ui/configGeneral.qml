pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

Item {
    id: root
    property string cfg_locationName: ""
    property real cfg_latitude: 0
    property real cfg_longitude: 0
    property string cfg_model: "um4_60"
    property bool cfg_locationNameDefault
    property bool cfg_latitudeDefault
    property bool cfg_longitudeDefault
    property bool cfg_modelDefault
    property string selectedId: ""
    Component.onCompleted: rememberSelection()
    onCfg_latitudeChanged: rememberSelection()
    onCfg_longitudeChanged: rememberSelection()
    readonly property bool isSaveAllowed: cfg_locationName.length > 0 && LocationRules.valid(cfg_latitude, cfg_longitude, cfg_model)
    PlacesDialog {
        id: places
        onLocationSelected: (name, latitude, longitude) => root.selectPlace({name: name, latitude: latitude, longitude: longitude})
    }
    function rememberSelection() {
        const place = UserPreferences.placeAt(cfg_latitude, cfg_longitude);
        if (place.id) selectedId = place.id;
    }
    Connections {
        target: UserPreferences
        function onPlacesChanged() {
            const place = UserPreferences.places.find(place => place.id === root.selectedId);
            if (place) root.selectPlace(place);
        }
    }
    function selectPlace(place) {
        cfg_locationName = place.name; cfg_latitude = place.latitude; cfg_longitude = place.longitude;
        rememberSelection();
        cfg_model = LocationRules.modelFor(place.latitude, place.longitude, cfg_model);
    }
    implicitWidth: form.implicitWidth
    implicitHeight: form.implicitHeight
    Kirigami.FormLayout {
        id: form
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        ComboBox {
            objectName: "widgetLocationSelector"
            Kirigami.FormData.label: I18n.i18n("Place:")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            model: UserPreferences.places
            textRole: "name"
            displayText: currentIndex >= 0 ? currentText : I18n.i18n("Choose location")
            currentIndex: root.cfg_locationName.length ? model.findIndex(place => LocationRules.samePlace(place.latitude, place.longitude, root.cfg_latitude, root.cfg_longitude)) : -1
            onActivated: index => root.selectPlace(model[index])
        }
        Button { text: I18n.i18n("Manage places…"); icon.name: "document-edit"; onClicked: places.open() }
        ComboBox {
            objectName: "widgetForecastModelSelector"
            Kirigami.FormData.label: I18n.i18n("Forecast model:")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            model: [I18n.i18n("UM4 · 60 hours"), I18n.i18n("UM4 · 120 hours"), I18n.i18n("GFS · 240 hours")]
            currentIndex: ["um4_60", "um4_120", "gfs"].indexOf(root.cfg_model)
            onActivated: index => root.cfg_model = LocationRules.modelFor(root.cfg_latitude, root.cfg_longitude, ["um4_60", "um4_120", "gfs"][index])
        }
    }
}
