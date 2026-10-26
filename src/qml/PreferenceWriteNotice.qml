pragma ComponentBehavior: Bound
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.InlineMessage {
    Layout.fillWidth: true
    text: UserPreferences.error
    visible: text.length > 0
    type: Kirigami.MessageType.Error
}
