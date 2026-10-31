pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "mobilePlacesPage"
    required property WeatherModel weather
    required property var navigationStack
    signal locationSelected(string name, real latitude, real longitude)
    title: I18n.i18n("My places")
    padding: Kirigami.Units.largeSpacing
    header: MobilePageHeader { title: root.title; onBackRequested: root.navigationStack.pop() }
    PlacesView {
        width: root.availableWidth
        weather: root.weather
        onCurrentLocationSelected: root.navigationStack.pop()
        onAddRequested: root.navigationStack.push(editorPage)
        onEditRequested: place => root.navigationStack.push(editorPage, {place: place})
        onLocationSelected: (name, latitude, longitude) => { root.locationSelected(name, latitude, longitude); root.navigationStack.pop(); }
    }
    Component {
        id: editorPage
        MobileLocationPage {
            navigationStack: root.navigationStack
            onPlaceSaved: (place, added) => {
                if (added) { root.locationSelected(place.name, place.latitude, place.longitude); root.navigationStack.pop(null); }
                else root.navigationStack.pop();
            }
        }
    }
}
