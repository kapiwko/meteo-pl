pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "calendarDialog"
    property date selectedDate: new Date()
    property date minimumDate: new Date(1997, 0, 1)
    property date maximumDate: new Date(2100, 11, 31)
    property int shownMonth: 0
    property int shownYear: 2026
    signal dateSelected(date value)
    function day(value) { return new Date(value.getFullYear(), value.getMonth(), value.getDate(), 12).getTime(); }
    function allowed(value) { return day(value) >= day(minimumDate) && day(value) <= day(maximumDate); }
    function shiftMonth(delta) {
        const next = new Date(shownYear, shownMonth + delta, 1, 12);
        shownYear = next.getFullYear(); shownMonth = next.getMonth();
    }
    title: I18n.i18n("Choose a date")
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    width: Math.min(Kirigami.Units.gridUnit * 23, parent ? parent.width - 24 : 414)
    height: Math.min(implicitHeight, parent ? parent.height - 24 : 600)
    standardButtons: Dialog.Cancel
    onOpened: { shownYear = selectedDate.getFullYear(); shownMonth = selectedDate.getMonth(); }
    contentItem: ColumnLayout {
        focus: true
        Keys.onShortcutOverride: event => { if (event.key === Qt.Key_Back) event.accepted = true; }
        Keys.onPressed: event => { if (event.key === Qt.Key_Back) { event.accepted = true; root.reject(); } }
        spacing: Kirigami.Units.smallSpacing
        RowLayout {
            Layout.fillWidth: true
            ToolButton {
                icon.name: "go-previous"
                Accessible.name: I18n.i18n("Previous month")
                enabled: root.shownYear * 12 + root.shownMonth > root.minimumDate.getFullYear() * 12 + root.minimumDate.getMonth()
                onClicked: root.shiftMonth(-1)
            }
            ComboBox {
                objectName: "calendarMonth"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: Array.from({length: 12}, (_, index) => Qt.locale().standaloneMonthName(index, Locale.LongFormat))
                currentIndex: root.shownMonth
                onActivated: index => root.shownMonth = index
                Accessible.name: I18n.i18n("Month")
            }
            ToolButton {
                icon.name: "go-next"
                Accessible.name: I18n.i18n("Next month")
                enabled: root.shownYear * 12 + root.shownMonth < root.maximumDate.getFullYear() * 12 + root.maximumDate.getMonth()
                onClicked: root.shiftMonth(1)
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Label { text: I18n.i18n("Year") }
            SpinBox {
                objectName: "calendarYear"
                from: root.minimumDate.getFullYear(); to: root.maximumDate.getFullYear()
                value: root.shownYear
                onValueModified: root.shownYear = value
                Accessible.name: I18n.i18n("Year")
            }
        }
        DayOfWeekRow { Layout.fillWidth: true; locale: grid.locale }
        MonthGrid {
            id: grid
            objectName: "calendarMonthGrid"
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 13
            month: root.shownMonth
            year: root.shownYear
            locale: Qt.locale()
            delegate: Label {
                id: calendarDay
                required property var model
                text: model.day
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                opacity: model.month === root.shownMonth && root.allowed(model.date) ? 1 : 0.3
                color: root.day(model.date) === root.day(root.selectedDate) ? root.palette.highlightedText : root.palette.text
                background: Rectangle { color: root.day(calendarDay.model.date) === root.day(root.selectedDate) ? root.palette.highlight : "transparent"; radius: Kirigami.Units.cornerRadius }
            }
            onClicked: date => { if (root.allowed(date)) { root.selectedDate = date; root.dateSelected(date); root.accept(); } }
        }
    }
}
