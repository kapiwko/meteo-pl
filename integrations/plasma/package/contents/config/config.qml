import QtQuick
import pl.meteo.desktop
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: I18n.i18n("Location")
        icon: "mark-location"
        source: "configGeneral.qml"
    }
}
