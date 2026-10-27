#include "ProfileFileCodec.hpp"
#include "ChartConfiguration.hpp"
#include "JsonSchemaValidator.hpp"
#include "core/Localization.hpp"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

std::optional<QVariantList> ProfileFileCodec::decode(const QByteArray &bytes, QString *error)
{
    const auto fail = [&](const QString &message) -> std::optional<QVariantList> {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (error)
        error->clear();
    if (bytes.size() > maximumBytes)
        return fail(Meteo::i18n("The profile file is too large (maximum 16 MiB)."));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(Meteo::i18n("The file does not contain a valid JSON document."));
    const auto root = document.object();
    if (root["format"] != "pl.meteo.chart-profiles" || root["version"].toDouble(-1) != 1)
        return fail(Meteo::i18n("Unsupported profile file format or version."));
    QString validationError;
    if (!validateChartJson(root, &validationError)) return fail(validationError);
    const auto input = root["profiles"].toArray();
    if (input.isEmpty() || input.size() > 100)
        return fail(Meteo::i18n("The file must contain between 1 and 100 profiles."));
    QVariantList profiles;
    for (const auto &value : input) {
        const auto entry = value.toObject();
        const auto name = entry["name"].toString().trimmed().left(80);
        const auto source = entry["charts"].toArray();
        const auto charts = normalizeChartLayout(source.toVariantList());
        bool valid =
            !name.isEmpty() && entry["charts"].isArray() && source.size() <= 50 && charts.size() == source.size();
        for (qsizetype i = 0; valid && i < source.size(); ++i) {
            const QJsonValue series = source.at(i).toObject().value("series");
            valid = series.isArray() && charts[i].toMap()["series"].toList().size() == series.toArray().size();
        }
        if (!valid)
            return fail(Meteo::i18n("Invalid profile definition or unknown series. No profiles were imported."));
        const auto height = entry["chartHeight"].toInt();
        QVariantMap profile{{"name", name}, {"charts", charts}, {"chartHeight", height ? std::clamp(height, 64, 800) : 0}};
        if (entry.contains("revision")) profile["revision"] = entry["revision"].toInt();
        profiles.append(profile);
    }
    return profiles;
}
std::optional<QByteArray> ProfileFileCodec::encode(const QVariantList &profiles, QString *error)
{
    if (error)
        error->clear();
    const auto bytes = QJsonDocument(QJsonObject{{"$schema", "https://raw.githubusercontent.com/kapiwko/meteo-pl/main/resources/schemas/chart-data.schema.json"},
                                                 {"format", "pl.meteo.chart-profiles"},
                                                 {"version", 1},
                                                 {"profiles", QJsonArray::fromVariantList(profiles)}})
                           .toJson(QJsonDocument::Indented);
    if (profiles.size() > 100 || bytes.size() > maximumBytes) {
        if (error)
            *error = Meteo::i18n("Too many profiles in one file. Export them separately.");
        return std::nullopt;
    }
    if (!decode(bytes, error))
        return std::nullopt;
    return bytes;
}
std::optional<QVariantList> ProfileFileCodec::read(const QUrl &url, QString *error)
{
    const auto fail = [&](const QString &message) -> std::optional<QVariantList> {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (!url.isLocalFile() || (QFileInfo(url.toLocalFile()).exists() && !QFileInfo(url.toLocalFile()).isFile()))
        return fail(Meteo::i18n("Choose a local file."));
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly))
        return fail(Meteo::i18n("Could not open the file: ") + file.errorString());
    if (file.size() > maximumBytes)
        return fail(Meteo::i18n("The profile file is too large (maximum 16 MiB)."));
    return decode(file.read(maximumBytes + 1), error);
}
bool ProfileFileCodec::write(const QUrl &url, const QVariantList &profiles, QString *error)
{
    if (!url.isLocalFile()) {
        if (error)
            *error = Meteo::i18n("Choose a local file.");
        return false;
    }
    const auto bytes = encode(profiles, error);
    if (!bytes)
        return false;
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(*bytes) != bytes->size() || !file.commit()) {
        if (error)
            *error = Meteo::i18n("Could not save the file: ") + file.errorString();
        return false;
    }
    return true;
}
