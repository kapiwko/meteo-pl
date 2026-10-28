pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

ForecastWindow {
    id: window
    pageStack.initialPage: Kirigami.Page {
        padding: 0
        title: I18n.i18n("Meteogram")
        actions: [
            Kirigami.Action { text: I18n.i18n("My places"); icon.name: "mark-location"; onTriggered: placesDialog.open() },
            Kirigami.Action { text: I18n.i18n("Settings"); icon.name: "configure"; onTriggered: forecastPanel.openSettings() },
            Kirigami.Action { text: I18n.i18n("Refresh"); icon.name: "view-refresh"; enabled: !window.weather.busy; onTriggered: window.weather.refresh() }
        ]
        ForecastPanel {
            id: forecastPanel
            anchors.fill: parent
            weather: window.weather
            onConfigureRequested: placesDialog.open()
            onAddLocationRequested: { locationDialog.place = ({}); locationDialog.open(); }
            onModelSelected: model => window.selectForecast(window.weather.locationName, window.weather.latitude, window.weather.longitude, model)
            onLocationSelected: (name, latitude, longitude) => window.selectLocation(name, latitude, longitude)
        }
    }

    PlacesDialog {
        id: placesDialog
        weather: window.weather
        onLocationSelected: (name, latitude, longitude) => window.selectLocation(name, latitude, longitude)
    }
    PlaceEditorDialog {
        id: locationDialog
        onPlaceSaved: (place, added) => { if (added) window.selectLocation(place.name, place.latitude, place.longitude); }
    }
}
