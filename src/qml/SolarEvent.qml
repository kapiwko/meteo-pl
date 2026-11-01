pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
RowLayout {
    id: event
    required property string title
    required property string time
    required property string icon
    property bool utc: false
    property string dateLabel: ""
    property bool compact: false
    spacing: Kirigami.Units.smallSpacing
    Accessible.role: Accessible.StaticText
    Accessible.name: title + I18n.i18n(": ") + time + (utc ? " UTC" : "")
    ToolTip.visible: eventHover.hovered
    ToolTip.text: title + " · " + dateLabel + I18n.i18n(" · approximate time · ") + (utc ? "UTC" : I18n.i18n("system local time"))
    HoverHandler { id: eventHover }
    Kirigami.Icon {
        source: Qt.resolvedUrl(event.icon)
        isMask: true
        color: Kirigami.Theme.neutralTextColor
        Layout.preferredWidth: event.compact ? Kirigami.Units.iconSizes.smallMedium : 32
        Layout.preferredHeight: Layout.preferredWidth
        Accessible.ignored: true
    }
    ColumnLayout {
        spacing: 0
        Label { visible: !event.compact; text: event.title + (event.utc ? " · UTC" : ""); color: Kirigami.Theme.disabledTextColor }
        Label { text: event.time; font.bold: true; font.pointSize: Kirigami.Theme.defaultFont.pointSize * (event.compact ? 1 : 1.4); Accessible.ignored: true }
    }
}
