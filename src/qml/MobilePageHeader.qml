pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as MaterialStyle
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ToolBar {
    id: root
    required property string title
    property bool showBack: true
    signal backRequested()
    implicitHeight: Math.max(48, implicitContentHeight + topPadding + bottomPadding)
    MaterialStyle.Material.background: Kirigami.Theme.backgroundColor
    MaterialStyle.Material.foreground: Kirigami.Theme.textColor
    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing
        ToolButton {
            objectName: "mobileBackButton"
            visible: root.showBack
            icon.name: "go-previous"
            implicitWidth: 48
            implicitHeight: 48
            Accessible.name: I18n.i18n("Back")
            onClicked: root.backRequested()
        }
        Kirigami.Heading {
            text: root.title
            level: 2
            Layout.fillWidth: true
            Layout.leftMargin: root.showBack ? 0 : Kirigami.Units.largeSpacing * 2
            Layout.rightMargin: Kirigami.Units.largeSpacing
            elide: Text.ElideRight
        }
    }
}
