#include "core/Localization.hpp"
#include "ReadingsExporter.hpp"
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <cmath>

QVariantMap ReadingsExporter::saveCsv(const QUrl &url, const QVariantList &readings, const QVariantMap &metadata) const
{
    const auto failure = [](const QString &error) -> QVariantMap { return {{"ok", false}, {"error", error}}; };
    if (!url.isLocalFile() || url.toLocalFile().isEmpty()) return failure(Meteo::i18n("Choose a local CSV file."));
    if (readings.isEmpty() || readings.size() > 1000) return failure(Meteo::i18n("No parameters to export."));
    const auto timestamp = metadata.value("timestamp").toDouble();
    if (!std::isfinite(timestamp) || timestamp <= 0 || timestamp > 253402300799.0)
        return failure(Meteo::i18n("No valid reading time."));
    auto time = QDateTime::fromSecsSinceEpoch(qint64(timestamp), QTimeZone::UTC);
    if (!metadata.value("utc").toBool()) time = time.toLocalTime();
    const auto quoted = [](QString text) { text.replace('"', QStringLiteral("\"\"")); return '"' + text + '"'; };
    QByteArray data("\xef\xbb\xbf"); // UTF-8 BOM lets desktop spreadsheets recognize Polish characters.
    const auto appendRow = [&](const QStringList &columns) {
        QStringList cells;
        for (const auto &column : columns) cells.append(quoted(column));
        data += cells.join(';').toUtf8() + "\r\n";
    };
    appendRow({Meteo::i18n("Timestamp"), Meteo::i18n("Location"), Meteo::i18n("Model"), Meteo::i18n("Model run"), Meteo::i18n("Reading scope"), Meteo::i18n("Category"), Meteo::i18n("Key"), Meteo::i18n("Parameter"), Meteo::i18n("Value"), Meteo::i18n("Unit"), Meteo::i18n("Description"), Meteo::i18n("Source"),
        Meteo::i18n("Sample time"), Meteo::i18n("Interval start"), Meteo::i18n("Interval end"), Meteo::i18n("Sampling")});
    const auto sourceTime = [&](const QVariant &timestamp) {
        if (!timestamp.isValid()) return QString();
        auto date = QDateTime::fromSecsSinceEpoch(timestamp.toLongLong(), QTimeZone::UTC);
        if (!metadata.value("utc").toBool()) date = date.toLocalTime();
        return date.toString(Qt::ISODate);
    };
    for (const auto &entry : readings) {
        const auto reading = entry.toMap();
        bool numeric = false;
        const auto value = reading.value("number").toDouble(&numeric);
        appendRow({time.toString(Qt::ISODate), metadata.value("location").toString(), metadata.value("model").toString(),
            metadata.value("run").toString(), metadata.value("scope").toString(), reading.value("group").toString(),
            reading.value("key").toString(), reading.value("label").toString(),
            numeric && std::isfinite(value) ? QLocale::c().toString(value, 'g', 15) : QString(),
            reading.value("unit").toString(), reading.value("value").toString(), metadata.value("source").toString(),
            sourceTime(reading.value("sourceTimestamp")), sourceTime(reading.value("intervalFrom")),
            sourceTime(reading.value("intervalTo")), reading.value("sampling").toString()});
    }
    auto path = url.toLocalFile();
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return failure(Meteo::i18n("Could not save the reading: ") + file.errorString());
    return {{"ok", true}, {"file", QUrl::fromLocalFile(path)}, {"path", path}};
}
