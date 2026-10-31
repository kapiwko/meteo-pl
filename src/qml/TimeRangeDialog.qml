pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "timeRangeDialog"
    property real minimum: 0
    property real maximum: 0
    property real startTime: minimum
    property real endTime: maximum
    property real draftStart: minimum
    property real draftEnd: maximum
    readonly property real minimumSpan: Math.min(86400, maximum - minimum)
    readonly property string validationError: draftStart < minimum || draftEnd > maximum
        ? I18n.i18n("Choose times within the available forecast.")
        : draftEnd <= draftStart ? I18n.i18n("The end must be after the start.")
        : draftEnd - draftStart < minimumSpan ? I18n.i18n("Select at least 24 hours.") : ""
    signal rangeSelected(real first, real last)
    title: I18n.i18n("Custom time interval")
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    width: Math.min(520, parent ? parent.width - 24 : 520)
    height: Math.min(implicitHeight, parent ? parent.height - 24 : 600)
    onOpened: { draftStart = startTime; draftEnd = endTime; }
    contentItem: ColumnLayout {
        focus: true
        Keys.onShortcutOverride: event => { if (event.key === Qt.Key_Back) event.accepted = true; }
        Keys.onPressed: event => { if (event.key === Qt.Key_Back) { event.accepted = true; root.reject(); } }
        spacing: Kirigami.Units.largeSpacing
        SettingsForm {
            id: intervalForm
            Layout.fillWidth: true
            DateTimeField {
                objectName: "intervalStart"
                Kirigami.FormData.label: I18n.i18n("From:")
                Layout.fillWidth: true
                Layout.preferredWidth: intervalForm.fieldWidth
                Layout.minimumWidth: 0
                minimum: root.minimum; maximum: root.maximum
                timestamp: root.draftStart
                onEdited: value => root.draftStart = value
            }
            DateTimeField {
                objectName: "intervalEnd"
                Kirigami.FormData.label: I18n.i18n("To:")
                Layout.fillWidth: true
                Layout.preferredWidth: intervalForm.fieldWidth
                Layout.minimumWidth: 0
                minimum: root.minimum; maximum: root.maximum
                timestamp: root.draftEnd
                onEdited: value => root.draftEnd = value
            }
        }
        SettingsHint { text: UserPreferences.utc ? I18n.i18n("Times are shown in UTC.") : I18n.i18n("Times are shown in your local time zone.") }
        Kirigami.InlineMessage { Layout.fillWidth: true; visible: root.validationError.length > 0; text: root.validationError; type: Kirigami.MessageType.Warning }
    }
    footer: DialogButtonBox {
        Button { objectName: "applyTimeRange"; text: I18n.i18n("Apply"); enabled: root.validationError.length === 0; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        Button { text: I18n.i18n("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
    }
    onAccepted: rangeSelected(draftStart, draftEnd)
}
