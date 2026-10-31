pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    id: root
    objectName: "settingsForm"
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    // Keep phone labels above their fields, regardless of label length.
    // The native layout still measures labels before choosing desktop columns.
    wideMode: !Kirigami.Settings.isMobile
        && width >= Math.max(implicitWidth, Kirigami.Units.gridUnit * 32)
    readonly property real fieldWidth: wideMode ? Kirigami.Units.gridUnit * 18
        : Math.min(width, Kirigami.Units.gridUnit * 32)
}
