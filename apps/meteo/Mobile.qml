pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as MaterialStyle
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

ApplicationWindow {
    id: window
    readonly property alias weather: session.weather
    property alias smokeTest: session.smokeTest
    property alias startupForecast: session.startupForecast
    property int colorScheme: Application.styleHints.colorScheme
    width: 420; height: 840
    minimumWidth: 320; minimumHeight: 440
    visible: true
    title: weather.locationName + " · Meteo"
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: colorScheme === Qt.Dark ? Kirigami.Theme.Complementary : Kirigami.Theme.Window
    MaterialStyle.Material.theme: colorScheme === Qt.Dark ? MaterialStyle.Material.Dark : MaterialStyle.Material.Light
    color: Kirigami.Theme.backgroundColor
    ForecastSession { id: session }
    StackView {
        id: navigation
        objectName: "mobilePageStack"
        anchors.fill: parent
        focus: true
        onCurrentItemChanged: if (currentItem) currentItem.forceActiveFocus()
        Keys.onReleased: event => { if (event.key === Qt.Key_Back) { navigation.pop(); event.accepted = true; } }
        initialItem: MobileForecastPage {
            weather: window.weather
            navigationStack: navigation
            onForecastSelected: (name, latitude, longitude, model) => session.selectForecast(name, latitude, longitude, model)
        }
    }
    onClosing: event => {
        if (navigation.depth > 1) { event.accepted = false; if (!navigation.busy) navigation.pop(); }
    }
    Shortcut {
        objectName: "mobileBackShortcut"
        context: Qt.ApplicationShortcut
        sequences: [StandardKey.Back, StandardKey.Cancel]
        enabled: navigation.depth > 1 && !navigation.busy
        onActivated: navigation.pop()
    }
}
