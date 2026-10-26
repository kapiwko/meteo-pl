import QtQuick
import pl.meteo.desktop
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: I18n.i18n("Location")
        icon: "mark-location"
        source: "configGeneral.qml"
    }
    ConfigCategory {
        name: I18n.i18n("Places in this widget")
        icon: "view-list-details"
        source: "configPlaces.qml"
    }
    ConfigCategory {
        name: I18n.i18n("Widget view")
        icon: "view-statistics"
        source: "configAppearance.qml"
    }
}
