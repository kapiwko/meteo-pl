pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import QtCore
import org.kde.kirigami as Kirigami

FileDialog {
    id: root
    required property var chart
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
    onAccepted: {
        const metadata = chart.weather.chartExportMetadata(chart.title, chart.startTime, chart.endTime);
        metadata.background = Kirigami.Theme.backgroundColor;
        metadata.foreground = Kirigami.Theme.textColor;
        exporter.savePng(selectedFile, [chart], metadata);
    }
    ChartExporter {
        id: exporter
        onSaved: (file, path) => ExportNotifications.saved(file, I18n.i18n("Charts exported"))
        onFailed: message => root.error = message
    }
}
