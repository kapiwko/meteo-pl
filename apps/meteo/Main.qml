pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

ForecastWindow {
    id: window
    pageStack.initialPage: Kirigami.Page {
        padding: 0
        title: I18n.i18n("Meteogram")
        actions: [
            Kirigami.Action { text: I18n.i18n("Location"); icon.name: "mark-location"; onTriggered: locationDialog.open() },
            Kirigami.Action { text: I18n.i18n("Settings"); icon.name: "configure"; onTriggered: forecastPanel.openSettings() },
            Kirigami.Action { text: I18n.i18n("Refresh"); icon.name: "view-refresh"; enabled: !window.weather.busy; onTriggered: window.weather.refresh() }
        ]
        ForecastPanel {
            id: forecastPanel
            anchors.fill: parent
            weather: window.weather
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
        onAboutToShow: locationEditor.resetDraft(window.weather.locationName, window.weather.latitude, window.weather.longitude, window.weather.model)
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
            window.selectForecast(locationEditor.locationName.trim(), locationEditor.latitude, locationEditor.longitude, locationEditor.model);
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
