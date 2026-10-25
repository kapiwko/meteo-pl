import QtQuick
import pl.meteo.desktop

LocationEditor {
    id: root
    property alias cfg_locationName: root.locationName
    property alias cfg_latitude: root.latitude
    property alias cfg_longitude: root.longitude
    property alias cfg_model: root.model
    property bool cfg_locationNameDefault
    property bool cfg_latitudeDefault
    property bool cfg_longitudeDefault
    property bool cfg_modelDefault
    readonly property bool isSaveAllowed: valid
}
