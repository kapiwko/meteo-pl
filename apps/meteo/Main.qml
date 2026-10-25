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
    minimumWidth: Kirigami.Settings.isMobile ? 320 : 400
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
        title: I18n.i18n("Meteogram")
        actions: [
            Kirigami.Action { text: I18n.i18n("Location"); icon.name: "mark-location"; onTriggered: locationDialog.open() },
            Kirigami.Action { text: I18n.i18n("Settings"); icon.name: "configure"; onTriggered: forecastPanel.openSettings() },
            Kirigami.Action { text: I18n.i18n("Refresh"); icon.name: "view-refresh"; enabled: !forecastModel.busy; onTriggered: forecastModel.refresh() }
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
        title: I18n.i18n("Location and model")
        anchors.centerIn: parent
        width: Math.min(window.width - 24, 660)
        padding: 0
        modal: true
        standardButtons: Dialog.Cancel
        onAboutToShow: locationEditor.resetDraft(settings.locationName, settings.latitude, settings.longitude, settings.model)
        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Cancel
            onAccepted: locationDialog.accept()
            onRejected: locationDialog.reject()
            Button {
                text: I18n.i18n("Show forecast")
                icon.name: "weather-few-clouds"
                enabled: locationEditor.valid
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: {
            settings.model = locationEditor.model;
            window.selectLocation(locationEditor.locationName.trim(), locationEditor.latitude, locationEditor.longitude);
        }

        contentItem: ScrollView {
            id: locationScroll
            implicitHeight: Math.min(locationContent.implicitHeight, window.height - 160)
            contentWidth: availableWidth
            clip: true
            Pane {
                id: locationContent
                objectName: "locationDialogContent"
                width: locationScroll.availableWidth
                padding: Kirigami.Units.largeSpacing
                background: null
                contentItem: LocationEditor { id: locationEditor }
            }
        }
    }
}
