pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    required property string profileId
    required property var chart
    property string saveError: ""
    readonly property var limits: [leftLow, leftHigh, rightLow, rightHigh]
    readonly property string validationError: {
        for (const field of limits) if (field.text.length && !field.acceptableInput) return I18n.i18n("Enter a valid axis limit.");
        for (const pair of [[leftLow, leftHigh], [rightLow, rightHigh]])
            if (pair[0].text.length && pair[1].text.length && number(pair[0].text) >= number(pair[1].text))
                return I18n.i18n("The minimum must be lower than the maximum.");
        return "";
    }
    function number(text) { return Number(text.replace(",", ".")); }
    function commit() {
        saveError = "";
        if (validationError.length) return;
        const options = {};
        for (const field of limits) options[field.bound] = field.text.length ? number(field.text) : null;
        if (!UserPreferences.setChartOptions(chart.key, options, profileId)) saveError = UserPreferences.error;
    }
    component LimitField: TextField {
        required property string bound
        objectName: "chartBound-" + root.chart.key + "-" + bound
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
        text: root.chart[bound] === undefined ? "" : String(root.chart[bound])
        placeholderText: I18n.i18n("Automatic")
        maximumLength: 40
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        validator: RegularExpressionValidator { regularExpression: /[+-]?(\d+([.,]\d*)?|[.,]\d+)/ }
        onEditingFinished: root.commit()
        Accessible.name: (bound.startsWith("left") ? I18n.i18n("Left axis") : I18n.i18n("Right axis")) + " · " + (bound.endsWith("Min") ? I18n.i18n("Minimum") : I18n.i18n("Maximum"))
    }
    SettingsForm {
        id: axesForm
        Layout.fillWidth: true
        RowLayout {
            Kirigami.FormData.label: I18n.i18n("Left axis")
            Layout.preferredWidth: axesForm.fieldWidth
            Layout.minimumWidth: 0
            Layout.fillWidth: true
            ColumnLayout { Layout.fillWidth: true; Layout.minimumWidth: 0; Label { text: I18n.i18n("Minimum") } LimitField { id: leftLow; bound: "leftMin" } }
            ColumnLayout { Layout.fillWidth: true; Layout.minimumWidth: 0; Label { text: I18n.i18n("Maximum") } LimitField { id: leftHigh; bound: "leftMax" } }
        }
        RowLayout {
            Kirigami.FormData.label: I18n.i18n("Right axis")
            Layout.preferredWidth: axesForm.fieldWidth
            Layout.minimumWidth: 0
            Layout.fillWidth: true
            ColumnLayout { Layout.fillWidth: true; Layout.minimumWidth: 0; Label { text: I18n.i18n("Minimum") } LimitField { id: rightLow; bound: "rightMin" } }
            ColumnLayout { Layout.fillWidth: true; Layout.minimumWidth: 0; Label { text: I18n.i18n("Maximum") } LimitField { id: rightHigh; bound: "rightMax" } }
        }
    }
    Kirigami.InlineMessage { Layout.fillWidth: true; visible: text.length > 0; text: root.validationError || root.saveError; type: Kirigami.MessageType.Warning }
    SettingsHint { Layout.fillWidth: true; text: I18n.i18n("Empty limits use automatic scaling. Use a dot or comma for decimals."); wrapMode: Text.WordWrap }
}
