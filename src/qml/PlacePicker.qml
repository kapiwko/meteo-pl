pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property WeatherModel weather
    property bool centered: false
    signal locationSelected(string name, real latitude, real longitude)
    Kirigami.Heading {
        objectName: "forecastLocationName"
        text: root.weather.locationName.length ? root.weather.locationName : "Wybierz lokalizację"
        visible: UserPreferences.places.length < 2
        level: 2
        Layout.fillWidth: true
        elide: Text.ElideRight
        horizontalAlignment: root.centered ? Text.AlignHCenter : Text.AlignLeft
    }
    ComboBox {
        objectName: "headerLocationSelector"
        visible: UserPreferences.places.length > 1
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.maximumWidth: 320
        Layout.alignment: root.centered ? Qt.AlignHCenter : Qt.AlignLeft
        model: UserPreferences.places
        textRole: "name"
        displayText: root.weather.locationName
        currentIndex: model.findIndex(place => Math.abs(place.latitude-root.weather.latitude) < 0.000001 && Math.abs(place.longitude-root.weather.longitude) < 0.000001)
        Accessible.name: "Moje miejsca · wybierz prognozę"
        onActivated: index => {
            const place = model[index];
            if (place) root.locationSelected(place.name, place.latitude, place.longitude);
        }
    }
}
