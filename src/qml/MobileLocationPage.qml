pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "mobileLocationPage"
    required property WeatherModel weather
    required property var navigationStack
    property bool addingPlace: false
    signal forecastSelected(string name, real latitude, real longitude, string model)
    title: addingPlace ? I18n.i18n("Add location") : I18n.i18n("Location and model")
    padding: Kirigami.Units.largeSpacing
    header: MobilePageHeader {
        title: root.title
        onBackRequested: root.navigationStack.pop()
    }
    LocationEditor {
        id: editor
        width: root.availableWidth
        Component.onCompleted: resetDraft(root.addingPlace ? "" : root.weather.locationName, root.weather.latitude, root.weather.longitude, root.weather.model)
    }
    footer: Pane {
        padding: Kirigami.Units.largeSpacing
        contentItem: Button {
            objectName: "mobileShowForecastButton"
            text: editor.savedPlace ? I18n.i18n("Show forecast") : I18n.i18n("Add and show forecast")
            icon.name: editor.savedPlace ? "weather-few-clouds" : "list-add"
            highlighted: true
            enabled: editor.valid
            onClicked: {
                root.forecastSelected(editor.locationName.trim(), editor.latitude, editor.longitude, editor.model);
                root.navigationStack.pop();
            }
        }
    }
}
