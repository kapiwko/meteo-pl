pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM
import pl.meteo.desktop

KCM.SimpleKCM {
    id: root
    property string cfg_popupView: "charts"
    property string cfg_chartProfile: "classic"
    property int cfg_chartHours: 24
    readonly property var chartCatalog: {
        const revision = UserPreferences.chartLayout;
        return UserPreferences.chartLayoutForProfile(cfg_chartProfile).filter(chart => chart.enabled);
    }
    readonly property bool isSaveAllowed: cfg_popupView !== "charts" || chartCatalog.length > 0
    Kirigami.FormLayout {
        ComboBox {
            Kirigami.FormData.label: I18n.i18n("Expanded view:")
            textRole: "name"
            model: [{id: "charts", name: I18n.i18n("Meteogram")}, {id: "forecast", name: I18n.i18n("Daily forecast")}]
            currentIndex: root.cfg_popupView === "forecast" ? 1 : 0
            onActivated: index => root.cfg_popupView = model[index].id
        }
        ComboBox {
            Kirigami.FormData.label: I18n.i18n("Chart profile:")
            enabled: root.cfg_popupView === "charts"
            model: UserPreferences.profiles
            textRole: "name"
            currentIndex: Math.max(0, model.findIndex(profile => profile.id === root.cfg_chartProfile))
            onActivated: index => root.cfg_chartProfile = model[index].id
        }
        Label {
            Kirigami.FormData.label: I18n.i18n("Charts:")
            enabled: root.cfg_popupView === "charts"
            text: I18n.i18n("All enabled charts from this profile are shown as collapsible sections.")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }
        ComboBox {
            Kirigami.FormData.label: I18n.i18n("Time interval:")
            enabled: root.cfg_popupView === "charts"
            model: [{hours: 12, name: I18n.i18n("12 hours")}, {hours: 24, name: I18n.i18n("24 hours")},
                {hours: 48, name: I18n.i18n("48 hours")}, {hours: 72, name: I18n.i18n("72 hours")}, {hours: 0, name: I18n.i18n("Entire forecast")}]
            textRole: "name"
            currentIndex: model.findIndex(entry => entry.hours === root.cfg_chartHours)
            onActivated: index => root.cfg_chartHours = model[index].hours
        }
    }
}
