pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

ForecastWindow {
    id: window
    property bool locationFallbackOffered: false
    function addFirstPlace(error = "") {
        locationDialog.firstRun = true;
        locationDialog.place = ({});
        locationDialog.setupError = error;
        locationDialog.open();
    }
    function beginSetup() {
        if (UserPreferences.places.length) placesDialog.open();
        else if (window.needsInitialSetup) welcomeDialog.open();
        else addFirstPlace();
    }
    function offerLocationFallback() {
        if (window.smokeTest || !window.initialLocationError.length || locationFallbackOffered) return;
        locationFallbackOffered = true;
        addFirstPlace(window.initialLocationError);
    }
    onInitialLocationErrorChanged: Qt.callLater(() => window.offerLocationFallback())
    Component.onCompleted: Qt.callLater(() => {
        if (window.initialLocationError.length) window.offerLocationFallback();
        else if (!window.smokeTest && !window.weather.locationName.length && !window.weather.currentLocation.enabled) window.beginSetup();
    })
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
            onAddLocationRequested: { locationDialog.firstRun = UserPreferences.places.length === 0; locationDialog.place = ({}); locationDialog.setupError = ""; locationDialog.open(); }
            onModelSelected: model => window.selectModel(model)
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
    WelcomeDialog {
        id: welcomeDialog
        selectedModel: window.weather.model
        onCompleted: options => {
            window.applyInitialSetup(options);
            if (!options.useCurrentLocation) window.addFirstPlace();
        }
    }
}
