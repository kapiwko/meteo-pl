pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import pl.meteo.desktop

Item {
    id: root
    property string cfg_popupView: "charts"
    property string cfg_chartProfile: "classic"
    property var cfg_chartKeys: ["temperature", "precipitation"]
    property int cfg_chartHours: 24
    readonly property var chartCatalog: {
        const revision = UserPreferences.chartLayout;
        return UserPreferences.chartLayoutForProfile(cfg_chartProfile).filter(chart => chart.enabled);
    }
    readonly property var selectedKeys: cfg_chartKeys.filter(key => chartCatalog.some(chart => chart.key === key))
    readonly property bool isSaveAllowed: cfg_popupView !== "charts" || (cfg_chartKeys.length >= 1 && cfg_chartKeys.length <= 2
        && cfg_chartKeys.every(key => chartCatalog.some(chart => chart.key === key)))
    implicitWidth: form.implicitWidth
    implicitHeight: form.implicitHeight
    Kirigami.FormLayout {
        id: form
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        ComboBox {
            Kirigami.FormData.label: I18n.i18n("Expanded view:")
            textRole: "name"
            model: [{id: "charts", name: I18n.i18n("Selected charts")}, {id: "forecast", name: I18n.i18n("Daily forecast")}]
            currentIndex: root.cfg_popupView === "forecast" ? 1 : 0
            onActivated: index => root.cfg_popupView = model[index].id
        }
        ComboBox {
            Kirigami.FormData.label: I18n.i18n("Chart profile:")
            enabled: root.cfg_popupView === "charts"
            model: UserPreferences.profiles
            textRole: "name"
            currentIndex: Math.max(0, model.findIndex(profile => profile.id === root.cfg_chartProfile))
            onActivated: index => {
                root.cfg_chartProfile = model[index].id;
                const valid = root.cfg_chartKeys.filter(key => root.chartCatalog.some(chart => chart.key === key));
                root.cfg_chartKeys = valid.length ? valid : root.chartCatalog.slice(0, 2).map(chart => chart.key);
            }
        }
        ColumnLayout {
            Kirigami.FormData.label: I18n.i18n("Charts:")
            enabled: root.cfg_popupView === "charts"
            Repeater {
                model: root.chartCatalog
                delegate: CheckBox {
                    required property var modelData
                    text: modelData.title
                    checked: root.selectedKeys.indexOf(modelData.key) >= 0
                    enabled: checked || root.selectedKeys.length < 2
                    onToggled: {
                        const keys = root.selectedKeys.filter(key => key !== modelData.key);
                        if (checked) keys.push(modelData.key);
                        root.cfg_chartKeys = keys;
                    }
                }
            }
            Label {
                text: I18n.i18n("Choose one or two charts. Their appearance comes from the selected profile.")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
            }
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
