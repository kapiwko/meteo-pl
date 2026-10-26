pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: root
    required property WeatherModel weather
    property bool centered: false
    property var places: UserPreferences.places
    function chooseLocation() {
        if (places.length > 1) {
            selector.forceActiveFocus();
            selector.popup.open();
        }
    }
    signal locationSelected(string name, real latitude, real longitude)
    signal addLocationRequested()
    Kirigami.Heading {
        objectName: "forecastLocationName"
        text: root.weather.locationName.length ? root.weather.locationName : I18n.i18n("Choose location")
        visible: root.places.length < 2
        level: 2
        Layout.fillWidth: true
        elide: Text.ElideRight
        horizontalAlignment: root.centered ? Text.AlignHCenter : Text.AlignLeft
    }
    ComboBox {
        id: selector
        objectName: "headerLocationSelector"
        visible: root.places.length > 1
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.maximumWidth: 320
        Layout.alignment: root.centered ? Qt.AlignHCenter : Qt.AlignLeft
        model: root.places
        textRole: "name"
        displayText: root.weather.locationName
        currentIndex: model.findIndex(place => LocationRules.samePlace(place.latitude, place.longitude, root.weather.latitude, root.weather.longitude))
        Accessible.name: I18n.i18n("My places · choose a forecast")
        onActivated: index => {
            const place = model[index];
            if (place) root.locationSelected(place.name, place.latitude, place.longitude);
        }
    }
    ToolButton {
        objectName: "addLocationButton"
        text: I18n.i18n("Add location")
        icon.name: "list-add"
        onClicked: root.addLocationRequested()
    }
}
