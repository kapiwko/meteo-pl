pragma ComponentBehavior: Bound
import QtQuick
import "AppInfo.js" as AppInfo
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    objectName: "meteoAboutDialog"
    property WeatherModel weather: null
    title: I18n.i18n("About")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(600, parent ? parent.width - 32 : 600)
    height: Math.min(560, parent ? parent.height - 32 : 560)
    padding: Kirigami.Units.largeSpacing
    modal: true
    standardButtons: Dialog.Close
    contentItem: ScrollView {
        id: scroll
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            objectName: "meteoAboutPage"
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing
            RowLayout {
                Kirigami.Icon { source: "pl.meteo.desktop"; Layout.preferredWidth: 64; Layout.preferredHeight: 64 }
                ColumnLayout {
                    Layout.fillWidth: true
                    Kirigami.Heading { text: "Meteo " + AppInfo.version; level: 1 }
                    Label { text: I18n.i18n("Application and Plasma widget for ICM UW meteograms"); Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            Kirigami.Separator { Layout.fillWidth: true }
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Label { Kirigami.FormData.label: I18n.i18n("Author:"); text: "Kamil Piwowarski" }
                Label {
                    objectName: "authorEmail"
                    Kirigami.FormData.label: I18n.i18n("Contact:")
                    text: "<a href='mailto:kapiwko@gmail.com'>kapiwko@gmail.com</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label { Kirigami.FormData.label: I18n.i18n("Copyright:"); text: "© 2026 Kamil Piwowarski" }
                Label {
                    Kirigami.FormData.label: I18n.i18n("License:")
                    text: "<a href='https://www.gnu.org/licenses/gpl-3.0.html'>" + AppInfo.license + "</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label {
                    Kirigami.FormData.label: I18n.i18n("Forecasts:")
                    text: "<a href='https://www.meteo.pl/'>ICM UW / meteo.pl</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label {
                    Kirigami.FormData.label: I18n.i18n("UM model:")
                    text: "<a href='https://www.metoffice.gov.uk/'>Met Office</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
            CheckBox { id: technical; text: I18n.i18n("Technical details") }
            TextArea {
                objectName: "settingsFilePath"
                visible: technical.checked
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitWidth: 0
                text: I18n.i18n("Settings file:") + "\n" + UserPreferences.settingsFile
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                background: null
            }
            Kirigami.Separator { Layout.fillWidth: true }
            Kirigami.Heading { text: I18n.i18n("Current forecast data"); level: 3 }
            Label {
                objectName: "forecastSourceInfo"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.weather && root.weather.hasData
                    ? I18n.i18n("Model: %1\nModel run: %2").arg(root.weather.modelName).arg(root.weather.runText)
                        + "\n" + (root.weather.usingCache ? I18n.i18n("From cache: ") : I18n.i18n("Last downloaded: ")) + root.weather.updatedText
                    : I18n.i18n("No forecast has been downloaded yet.")
            }
        }
    }
}
