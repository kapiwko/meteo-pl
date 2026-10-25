pragma ComponentBehavior: Bound
import QtQml

QtObject {
    id: root
    required property WeatherModel weather
    property bool ready: false
    property bool collectionChanged: false
    signal locationSelected(string name, real latitude, real longitude)
    function currentPlace() {
        return UserPreferences.places.find(place => Math.abs(place.latitude-weather.latitude) < 0.000001 && Math.abs(place.longitude-weather.longitude) < 0.000001);
    }
    function rememberCurrent() {
        if (!ready) return;
        if (!weather.locationName.length) { reconcile(); return; }
        const saved = currentPlace();
        if (saved) {
            if (saved.name !== weather.locationName) locationSelected(saved.name, saved.latitude, saved.longitude);
        } else UserPreferences.savePlace(weather.locationName, weather.latitude, weather.longitude);
    }
    function reconcile() {
        if (!ready) return;
        const saved = currentPlace();
        if (saved && weather.locationName.length) {
            if (saved.name !== weather.locationName) locationSelected(saved.name, saved.latitude, saved.longitude);
            return;
        }
        const first = UserPreferences.places[0];
        if (first) locationSelected(first.name, first.latitude, first.longitude);
        else if (weather.locationName.length) locationSelected("", 0, 0);
    }
    function synchronize() {
        const changed = collectionChanged;
        collectionChanged = false;
        if (changed) reconcile(); else rememberCurrent();
    }
    Component.onCompleted: { ready = true; rememberCurrent(); }
    property QtObject placesConnection: Connections {
        target: UserPreferences
        function onPlacesChanged() { if (root.ready) { root.collectionChanged = true; Qt.callLater(root.synchronize); } }
    }
    property QtObject weatherConnection: Connections {
        target: root.weather
        function onLocationChanged() { if (root.ready) Qt.callLater(root.synchronize); }
    }
}
