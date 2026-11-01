#include "ChartConfiguration.hpp"
#include "ProfileFileCodec.hpp"
#include "JsonSchemaValidator.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

QVariantList builtinChartProfiles()
{
    static const QVariantList profiles = [] {
        const auto files = bundledChartJson(":/meteo/profiles/index.json").array();
        if (files.isEmpty()) qFatal("Empty bundled profile index");
        QVariantList result;
        QSet<QString> ids;
        for (const auto &name : files) {
            const auto fileName = name.toString();
            const auto bytes = bundledChartJson(":/meteo/profiles/" + fileName).toJson();
            QString error;
            const auto decoded = ProfileFileCodec::decode(bytes, &error);
            const auto id = QJsonDocument::fromJson(bytes).object()["profiles"].toArray().first().toObject()["id"].toString();
            if (!decoded || decoded->size() != 1 || id.isEmpty() || id == "custom" || id.startsWith("user:")
                || ids.contains(id) || fileName != id + ".json")
                qFatal("Invalid bundled profile %s: %s", qPrintable(fileName), qPrintable(error));
            auto profile = decoded->first().toMap();
            profile["id"] = id;
            const auto revision = QJsonDocument::fromJson(bytes).object()["profiles"].toArray().first().toObject()["revision"].toInt();
            if (revision < 1) qFatal("Missing bundled profile revision: %s", qPrintable(fileName));
            profile["revision"] = revision;
            result.append(profile);
            ids.insert(id);
        }
        return result;
    }();
    return profiles;
}

QVariantList defaultChartLayout(const QString &id)
{
    const auto profiles = builtinChartProfiles();
    for (const auto &entry : profiles)
        if (entry.toMap()["id"] == id) return entry.toMap()["charts"].toList();
    return profiles.first().toMap()["charts"].toList();
}
