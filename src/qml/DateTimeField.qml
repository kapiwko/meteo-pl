pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property real timestamp: 0
    property real minimum: 0
    property real maximum: 4102444800
    property bool utc: UserPreferences.utc
    signal edited(real value)
    readonly property date value: new Date(timestamp * 1000)
    readonly property int hour: utc ? value.getUTCHours() : value.getHours()
    function calendarDate(stamp) {
        const date = new Date(stamp * 1000);
        return utc ? new Date(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate(), 12) : date;
    }
    function combine(date, hour) {
        return (utc ? Date.UTC(date.getFullYear(), date.getMonth(), date.getDate(), hour) : new Date(date.getFullYear(), date.getMonth(), date.getDate(), hour).getTime()) / 1000;
    }
    Button {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        icon.name: "view-calendar-day"
        text: Qt.formatDate(root.calendarDate(root.timestamp), Qt.locale().dateFormat(Locale.ShortFormat))
        onClicked: calendar.open()
    }
    SpinBox {
        objectName: root.objectName + "Hour"
        from: 0; to: 23
        value: root.hour
        textFromValue: value => String(value).padStart(2, "0") + ":00"
        onValueModified: root.edited(root.combine(root.calendarDate(root.timestamp), value))
        Accessible.name: I18n.i18n("Hour")
    }
    CalendarDialog {
        id: calendar
        selectedDate: root.calendarDate(root.timestamp)
        minimumDate: root.calendarDate(root.minimum)
        maximumDate: root.calendarDate(root.maximum)
        onDateSelected: value => root.edited(root.combine(value, root.hour))
    }
}
