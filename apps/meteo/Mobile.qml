pragma ComponentBehavior: Bound
import QtQuick
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

ForecastWindow {
    id: window
    width: 420
    height: 840
    minimumWidth: 320
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.popHiddenPages: true
    pageStack.initialPage: MobileForecastPage {
        weather: window.weather
        navigationStack: window.pageStack
        onForecastSelected: (name, latitude, longitude, model) => window.selectForecast(name, latitude, longitude, model)
    }
}
