pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

ForecastWindow {
    id: window
    property int colorScheme: Application.styleHints.colorScheme
    width: 420
    height: 840
    minimumWidth: 320
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: colorScheme === Qt.Dark
        ? Kirigami.Theme.Complementary : Kirigami.Theme.Window
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.popHiddenPages: true
    pageStack.initialPage: MobileForecastPage {
        weather: window.weather
        navigationStack: window.pageStack
        onForecastSelected: (name, latitude, longitude, model) => window.selectForecast(name, latitude, longitude, model)
    }
}
