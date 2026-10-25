import QtQuick
import pl.meteo.desktop

Item {
    id: root
    // ColumnView adds its separator as a child of each page. A layout at this
    // boundary would try to position that anchored framework item as a field.
    implicitWidth: editor.implicitWidth
    implicitHeight: editor.implicitHeight
    property alias locationName: editor.locationName
    property alias latitude: editor.latitude
    property alias longitude: editor.longitude
    property alias model: editor.model
    property alias cfg_locationName: editor.locationName
    property alias cfg_latitude: editor.latitude
    property alias cfg_longitude: editor.longitude
    property alias cfg_model: editor.model
    property bool cfg_locationNameDefault
    property bool cfg_latitudeDefault
    property bool cfg_longitudeDefault
    property bool cfg_modelDefault
    readonly property bool isSaveAllowed: editor.valid
    LocationEditor {
        id: editor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }
}
