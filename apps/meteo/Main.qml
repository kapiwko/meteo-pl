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
    pageStack.initialPage: forecastPage

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
        autoRefresh: !window.smokeTest
    }
    Component {
        id: forecastPage
        Kirigami.Page {
            title: "Meteogram"
            actions: [
                Kirigami.Action { text: "Lokalizacja"; icon.name: "mark-location"; onTriggered: locationDialog.open() },
                Kirigami.Action { text: "Odśwież"; icon.name: "view-refresh"; enabled: !forecastModel.busy; onTriggered: forecastModel.refresh() },
                Kirigami.Action { text: "meteo.pl"; icon.name: "internet-web-browser"; onTriggered: Qt.openUrlExternally("https://app.meteo.pl/meteogram") }
            ]
            ForecastPanel { anchors.fill: parent; weather: forecastModel; onConfigureRequested: locationDialog.open() }
        }
    }
    Dialog {
        id: locationDialog
        title: "Lokalizacja i model"
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 440)
        height: Math.min(window.height - 32, implicitHeight)
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            locationEditor.locationName = settings.locationName;
            locationEditor.latitude = settings.latitude;
            locationEditor.longitude = settings.longitude;
            locationEditor.model = settings.model;
            standardButton(Dialog.Save).enabled = Qt.binding(() => locationEditor.valid);
        }
        onAccepted: {
            settings.locationName = locationEditor.locationName.trim();
            settings.latitude = locationEditor.latitude;
            settings.longitude = locationEditor.longitude;
            settings.model = locationEditor.model;
        }
        contentItem: ScrollView {
            contentWidth: availableWidth
            clip: true
            LocationEditor { id: locationEditor; width: parent.width }
        }
    }
}
