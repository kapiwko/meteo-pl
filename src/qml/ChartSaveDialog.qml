pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import QtCore
import org.kde.kirigami as Kirigami

FileDialog {
    id: root
    required property Item chart
    property string error: ""
    title: I18n.i18n("Save chart as…")
    fileMode: FileDialog.SaveFile
    nameFilters: [I18n.i18n("PNG image (*.png)")]
    defaultSuffix: "png"
    currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
    function choose() {
        if (exporter.busy) return;
        error = "";
        selectedFile = exporter.suggestedFile(chart.weather.locationName, chart.title,
            chart.weather.formatTime(chart.startTime, "yyyyMMdd-HHmm"));
        open();
    }
    onAccepted: exporter.savePng(selectedFile, [chart], {
        title: chart.weather.locationName + " · " + chart.title,
        details: chart.weather.modelName + I18n.i18n(" · model run: ") + chart.weather.runText + "\n"
            + chart.weather.formatTime(chart.startTime, "d MMM yyyy HH:mm") + " – " + chart.weather.formatTime(chart.endTime, "d MMM yyyy HH:mm")
            + (chart.weather.utc ? " UTC" : I18n.i18n(" · local time")) + "\n"
            + chart.weather.latitude.toFixed(4) + "°, " + chart.weather.longitude.toFixed(4) + "°",
        source: "ICM UW / meteo.pl · https://www.meteo.pl/ · UM: Met Office · " + I18n.i18n("Downloaded: %1").arg(chart.weather.updatedText),
        background: Kirigami.Theme.backgroundColor, foreground: Kirigami.Theme.textColor
    })
    ChartExporter {
        id: exporter
        onSaved: (file, path) => ExportNotifications.saved(file, I18n.i18n("Charts exported"))
        onFailed: message => root.error = message
    }
}
