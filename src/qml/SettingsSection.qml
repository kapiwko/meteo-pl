pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

GroupBox {
    id: root
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    padding: 0
    topPadding: label.implicitHeight + Kirigami.Units.smallSpacing
    background: null
    label: RowLayout {
        width: root.width
        spacing: Kirigami.Units.smallSpacing
        Kirigami.Heading {
            text: root.title
            level: 3
            wrapMode: Text.WordWrap
            Layout.maximumWidth: root.width
        }
        Kirigami.Separator { Layout.fillWidth: true }
    }
}
