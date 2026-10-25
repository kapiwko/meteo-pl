pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtCore
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

Kirigami.ApplicationWindow {
    id: window
    property bool smokeTest: false
    width: 860
    height: 900
    minimumWidth: 400
    minimumHeight: 440
    visible: true
    title: forecastModel.locationName + " · Meteo"

    Settings {
        id: settings
        category: "Location"
        property string locationName: "Warszawa"
        property real latitude: 52.2297
        property real longitude: 21.0122
        property string model: "um4_60"
    }
    WeatherModel {
        id: forecastModel
        locationName: settings.locationName
        latitude: settings.latitude
        longitude: settings.longitude
        model: settings.model
        autoRefresh: !window.smokeTest && settings.locationName.length > 0
        windUnit: UserPreferences.windUnit
        utc: UserPreferences.utc
    }
    function selectLocation(name, latitude, longitude) {
        settings.locationName = name; settings.latitude = latitude; settings.longitude = longitude;
        if (name.length && !(latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176)) settings.model = "gfs";
        if (name.length) UserPreferences.rememberPlace(name, latitude, longitude);
    }
    SavedPlaceSelection { weather: forecastModel; onLocationSelected: (name, latitude, longitude) => window.selectLocation(name, latitude, longitude) }
    pageStack.initialPage: Kirigami.Page {
        padding: 0
        title: "Meteogram"
        actions: [
            Kirigami.Action { text: "Lokalizacja"; icon.name: "mark-location"; onTriggered: locationDialog.open() },
            Kirigami.Action { text: "Ustawienia"; icon.name: "configure"; onTriggered: forecastPanel.openSettings() },
            Kirigami.Action { text: "Odśwież"; icon.name: "view-refresh"; enabled: !forecastModel.busy; onTriggered: forecastModel.refresh() }
        ]
        ForecastPanel {
            id: forecastPanel
            anchors.fill: parent
            weather: forecastModel
            onConfigureRequested: locationDialog.open()
            onLocationSelected: (name, latitude, longitude) => window.selectLocation(name, latitude, longitude)
        }
    }

    Dialog {
        id: locationDialog
        objectName: "locationDialog"
        title: "Lokalizacja i model"
        anchors.centerIn: parent
        width: Math.min(window.width - 40, 620)
        padding: 20
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            locationEditor.resetDraft(settings.locationName, settings.latitude, settings.longitude, settings.model);
            standardButton(Dialog.Save).text = "Pokaż prognozę";
            standardButton(Dialog.Save).enabled = Qt.binding(() => locationEditor.valid);
        }
        onAccepted: {
            settings.model = locationEditor.model;
            window.selectLocation(locationEditor.locationName.trim(), locationEditor.latitude, locationEditor.longitude);
        }

        contentItem: ScrollView {
            id: locationScroll
            implicitHeight: Math.min(locationEditor.implicitHeight, window.height - 220)
            contentWidth: availableWidth
            clip: true
            LocationEditor { id: locationEditor; width: locationScroll.availableWidth }
        }
    }
}
