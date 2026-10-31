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
    property int colorScheme: UserPreferences.theme === "dark" ? Qt.Dark : UserPreferences.theme === "light" ? Qt.Light : Application.styleHints.colorScheme
    width: 420; height: 840
    minimumWidth: 320; minimumHeight: 440
    visible: true
    title: weather.locationName.length ? weather.locationName + " · Meteo" : "Meteo"
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: colorScheme === Qt.Dark ? Kirigami.Theme.Complementary : Kirigami.Theme.Window
    MaterialStyle.Material.theme: colorScheme === Qt.Dark ? MaterialStyle.Material.Dark : MaterialStyle.Material.Light
    color: Kirigami.Theme.backgroundColor
    // Kirigami follows visual ancestors; a Window is outside that item tree.
    Binding { target: window.contentItem.Kirigami.Theme; property: "inherit"; value: false }
    Binding { target: window.contentItem.Kirigami.Theme; property: "colorSet"; value: window.colorScheme === Qt.Dark ? Kirigami.Theme.Complementary : Kirigami.Theme.Window }
    Binding { target: window.contentItem.Kirigami.Theme; property: "disabledTextColor"; value: Qt.alpha(window.Kirigami.Theme.textColor, 0.6) }
    readonly property bool canNavigateBack: Qt.inputMethod.visible || Qt.inputMethod.animating || navigation.depth > 1 || forecastPage.canNavigateBack
    function navigateBack() {
        if (Qt.inputMethod.visible || Qt.inputMethod.animating) { Qt.inputMethod.hide(); return; }
        if (navigation.busy) return;
        if (navigation.depth > 1 && navigation.currentItem.canNavigateBack) navigation.currentItem.navigateBack();
        else if (navigation.depth > 1) navigation.pop();
        else forecastPage.navigateBack();
    }
    ApplicationTheme { mode: UserPreferences.theme }
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
            initialLocationError: session.initialLocationError
            welcomeRequired: session.needsInitialSetup
            onInitialSetupCompleted: options => session.applyInitialSetup(options)
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
