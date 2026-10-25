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
    title: "O aplikacji"
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
                    Label { text: "Aplikacja i plasmoid z meteogramami ICM UW"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            Kirigami.Separator { Layout.fillWidth: true }
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Label { Kirigami.FormData.label: "Autor:"; text: "Kamil Piwowarski" }
                Label {
                    objectName: "authorEmail"
                    Kirigami.FormData.label: "Kontakt:"
                    text: "<a href='mailto:kapiwko@gmail.com'>kapiwko@gmail.com</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label { Kirigami.FormData.label: "Prawa autorskie:"; text: "© 2026 Kamil Piwowarski" }
                Label {
                    Kirigami.FormData.label: "Licencja:"
                    text: "<a href='https://www.gnu.org/licenses/gpl-3.0.html'>" + AppInfo.license + "</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label {
                    Kirigami.FormData.label: "Prognozy:"
                    text: "<a href='https://www.meteo.pl/'>ICM UW / meteo.pl</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
                Label {
                    Kirigami.FormData.label: "Model UM:"
                    text: "<a href='https://www.metoffice.gov.uk/'>Met Office</a>"
                    onLinkActivated: link => Qt.openUrlExternally(link)
                }
            }
            Kirigami.Separator { Layout.fillWidth: true }
            Kirigami.Heading { text: "Dane bieżącej prognozy"; level: 3 }
            Label {
                objectName: "forecastSourceInfo"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.weather && root.weather.hasData
                    ? "Model: " + root.weather.modelName + "\nObliczenia: " + root.weather.runText
                        + "\n" + (root.weather.usingCache ? "Z pamięci: " : "Ostatnie pobranie: ") + root.weather.updatedText
                    : "Nie pobrano jeszcze prognozy."
            }
        }
    }
}
