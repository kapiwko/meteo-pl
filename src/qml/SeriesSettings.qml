pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    objectName: "series-settings-" + chartKey + "/" + seriesStyle.key
    required property string chartKey
    required property var seriesStyle
    property bool expanded: false
    property bool available: true
    signal toggleAppearance()
    spacing: Kirigami.Units.largeSpacing * 1.5
    RowLayout {
        Layout.fillWidth: true
        CheckBox {
            Accessible.name: root.seriesStyle.label
            checked: root.seriesStyle.enabled
            onToggled: UserPreferences.setChartField(root.chartKey, root.seriesStyle.key, checked)
        }
        SeriesLegend { seriesStyle: root.seriesStyle; Layout.preferredWidth: 32; opacity: root.seriesStyle.enabled ? 1 : 0.3 }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.seriesStyle.label + (root.seriesStyle.unit ? " (" + root.seriesStyle.unit + ")" : "")
                + (root.available ? "" : " · " + I18n.i18n("no data"))
            wrapMode: Text.WordWrap
        }
        ToolButton {
            text: I18n.i18n("Appearance")
            icon.name: "configure"
            display: root.width < 520 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
            ToolTip.text: I18n.i18n("Series appearance")
            ToolTip.visible: hovered
            Accessible.name: ToolTip.text
            checkable: true
            checked: root.expanded
            onClicked: root.toggleAppearance()
        }
        ToolButton {
            icon.name: "list-remove"
            text: I18n.i18n("Remove series")
            display: AbstractButton.IconOnly
            ToolTip.text: text
            ToolTip.visible: hovered
            onClicked: UserPreferences.removeSeries(root.chartKey, root.seriesStyle.key)
        }
    }
}
