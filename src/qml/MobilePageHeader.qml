pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material as MaterialStyle
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ToolBar {
    id: root
    required property string title
    signal backRequested()
    MaterialStyle.Material.background: Kirigami.Theme.backgroundColor
    MaterialStyle.Material.foreground: Kirigami.Theme.textColor
    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing
        ToolButton {
            objectName: "mobileBackButton"
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
            Layout.rightMargin: Kirigami.Units.largeSpacing
            elide: Text.ElideRight
        }
    }
}
