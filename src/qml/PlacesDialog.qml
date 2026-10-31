pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "placesDialog"
    property WeatherModel weather: null
    signal locationSelected(string name, real latitude, real longitude)
    title: I18n.i18n("My places")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, parent ? parent.width - 24 : 640)
    height: Math.min(implicitHeight, parent ? parent.height - 24 : 760, 760)
    padding: 0
    modal: true
    standardButtons: Dialog.Close
    PlaceEditorDialog {
        id: editor
        objectName: "managedPlaceEditor"
        onPlaceSaved: (place, added) => {
            if (added) { root.locationSelected(place.name, place.latitude, place.longitude); root.close(); }
        }
    }
    contentItem: ScrollView {
        id: scroll
        implicitWidth: 0
        implicitHeight: content.implicitHeight
        contentWidth: availableWidth
        clip: true
        Pane {
            id: content
            width: scroll.availableWidth
            padding: Kirigami.Units.largeSpacing * 2
            background: null
            contentItem: PlacesView {
                weather: root.weather
                onCurrentLocationSelected: root.close()
                onAddRequested: { editor.firstRun = UserPreferences.places.length === 0; editor.place = ({}); editor.open(); }
                onEditRequested: place => { editor.firstRun = false; editor.place = place; editor.open(); }
                onLocationSelected: (name, latitude, longitude) => { root.locationSelected(name, latitude, longitude); root.close(); }
            }
        }
    }
}
