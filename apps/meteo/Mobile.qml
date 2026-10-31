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
    title: weather.locationName.length ? weather.locationName + " · Meteo" : "Meteo"
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: colorScheme === Qt.Dark ? Kirigami.Theme.Complementary : Kirigami.Theme.Window
    MaterialStyle.Material.theme: colorScheme === Qt.Dark ? MaterialStyle.Material.Dark : MaterialStyle.Material.Light
    color: Kirigami.Theme.backgroundColor
    readonly property bool canNavigateBack: Qt.inputMethod.visible || Qt.inputMethod.animating || navigation.depth > 1 || forecastPage.canNavigateBack
    function navigateBack() {
        if (Qt.inputMethod.visible || Qt.inputMethod.animating) { Qt.inputMethod.hide(); return; }
        if (navigation.busy) return;
        if (navigation.depth > 1) navigation.pop();
        else forecastPage.navigateBack();
    }
    ForecastSession { id: session }
    StackView {
        id: navigation
        objectName: "mobilePageStack"
        anchors.fill: parent
        focus: true
        onCurrentItemChanged: if (currentItem) currentItem.forceActiveFocus()
        initialItem: MobileForecastPage {
            id: forecastPage
            weather: window.weather
            offerSetup: !window.smokeTest
            navigationStack: navigation
            onModelSelected: model => session.selectModel(model)
            onForecastSelected: (name, latitude, longitude, model) => session.selectForecast(name, latitude, longitude, model)
        }
    }
    onClosing: event => {
        if (window.canNavigateBack) { event.accepted = false; window.navigateBack(); }
    }
    Shortcut {
        objectName: "mobileBackShortcut"
        context: Qt.ApplicationShortcut
        sequences: ["Back", "Alt+Left", "Escape"]
        enabled: window.canNavigateBack
        onActivated: window.navigateBack()
    }
}
