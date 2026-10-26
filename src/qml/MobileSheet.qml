pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Drawer {
    id: root
    required property string title
    property Component footer
    default property alias sheetContent: body.data
    parent: Overlay.overlay
    edge: Qt.BottomEdge
    width: parent ? parent.width : 0
    height: Math.min(implicitHeight, parent ? parent.height * 0.8 : implicitHeight)
    implicitHeight: layout.implicitHeight + topPadding + bottomPadding
    padding: Kirigami.Units.largeSpacing
    modal: true
    contentItem: ColumnLayout {
        id: layout
        spacing: Kirigami.Units.largeSpacing
        Label { text: root.title; font.bold: true; font.pixelSize: 20; Layout.fillWidth: true; wrapMode: Text.WordWrap; Accessible.role: Accessible.Heading }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: body.implicitHeight
            contentWidth: availableWidth
            clip: true
            ColumnLayout { id: body; width: scroll.availableWidth; spacing: 0 }
        }
        Loader { Layout.fillWidth: true; visible: item !== null; sourceComponent: root.footer }
    }
}
