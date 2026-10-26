pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "mobileLocationPage"
    required property WeatherModel weather
    required property var navigationStack
    signal forecastSelected(string name, real latitude, real longitude, string model)
    title: I18n.i18n("Location and model")
    padding: Kirigami.Units.largeSpacing
    header: MobilePageHeader {
        title: root.title
        onBackRequested: root.navigationStack.pop()
    }
    LocationEditor {
        id: editor
        width: root.availableWidth
        Component.onCompleted: resetDraft(root.weather.locationName, root.weather.latitude, root.weather.longitude, root.weather.model)
    }
    footer: Pane {
        padding: Kirigami.Units.largeSpacing
        contentItem: Button {
            objectName: "mobileShowForecastButton"
            text: I18n.i18n("Show forecast")
            icon.name: "weather-few-clouds"
            highlighted: true
            enabled: editor.valid
            onClicked: {
                root.forecastSelected(editor.locationName.trim(), editor.latitude, editor.longitude, editor.model);
                root.navigationStack.pop();
            }
        }
    }
}
