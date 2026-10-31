pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Label {
    objectName: "settingsHint"
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    // Paragraphs wrap to the field width instead of widening the whole form.
    Layout.preferredWidth: 1
    wrapMode: Text.WordWrap
    color: Kirigami.Theme.disabledTextColor
}
