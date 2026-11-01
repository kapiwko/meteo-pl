#include "PreferencesMigration.hpp"
#include "ProfileCatalog.hpp"
#include "ChartConfiguration.hpp"
#include "core/Locations.hpp"
#include <QJsonDocument>
#include <QScopeGuard>

void migratePreferences(QVariantMap &state)
{
    ProfileValues values{state};
    const auto commit=qScopeGuard([&]{state=values.values;});
    if (values.value("display/layoutVersion", 0).toInt() < 2) {
        values.setValue("display/profile", "classic");
        values.setValue("display/layoutVersion", 2);
    }
    if (values.value("display/layoutVersion", 0).toInt() < 3) {
        if (values.contains("display/customLayout")) {
            const auto old = QJsonDocument::fromJson(values.value("display/customLayout").toByteArray()).toVariant().toList();
            values.setValue("display/customLayout",
                     QJsonDocument::fromVariant(integrateWindDirection(old)).toJson(QJsonDocument::Compact));
        }
        auto saved = QJsonDocument::fromJson(values.value("display/savedProfiles").toByteArray()).toVariant().toList();
        for (auto &value : saved) {
            auto entry = value.toMap();
            entry["charts"] = integrateWindDirection(entry["charts"].toList());
            value = entry;
        }
        if (!saved.isEmpty())
            values.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        values.setValue("display/layoutVersion", 3);
    }
    if (values.value("display/layoutVersion", 0).toInt() < 4) {
        if (values.contains("display/customLayout")) {
            const auto old = QJsonDocument::fromJson(values.value("display/customLayout").toByteArray()).toVariant().toList();
            values.setValue("display/customLayout",
                     QJsonDocument::fromVariant(removeUnusedChartTemplates(old)).toJson(QJsonDocument::Compact));
        }
        auto saved = QJsonDocument::fromJson(values.value("display/savedProfiles").toByteArray()).toVariant().toList();
        for (auto &value : saved) {
            auto entry = value.toMap();
            entry["charts"] = removeUnusedChartTemplates(entry["charts"].toList());
            value = entry;
        }
        if (!saved.isEmpty())
            values.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        values.setValue("display/layoutVersion", 4);
    }
    if (values.value("places/listVersion", 0).toInt() < 1) {
        auto places = QJsonDocument::fromJson(values.value("places/favorites").toByteArray()).toVariant().toList();
        const auto recent = QJsonDocument::fromJson(values.value("places/recent").toByteArray()).toVariant().toList();
        for (const auto &value : recent) {
            const auto candidate = value.toMap();
            bool found = false;
            for (const auto &entry : places) {
                const auto saved = entry.toMap();
                if (Meteo::samePlace(saved["latitude"].toDouble(), saved["longitude"].toDouble(),
                                     candidate["latitude"].toDouble(), candidate["longitude"].toDouble())) {
                    found = true;
                    break;
                }
            }
            if (!found && !candidate["name"].toString().isEmpty())
                places.append(candidate);
        }
        values.setValue("places/favorites", QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
        values.setValue("places/listVersion", 1);
    }
    if (values.value("places/listVersion", 0).toInt() < 2) {
        auto places = QJsonDocument::fromJson(values.value("places/favorites").toByteArray()).toVariant().toList();
        for (auto &entry : places) {
            auto place = entry.toMap();
            // Existing Plasma selections use these coordinate keys. Retain
            // them as stable IDs when migrating so moving a place keeps it selected.
            if (place.value("id").toString().isEmpty())
                place["id"] = QString::number(place["latitude"].toDouble(), 'f', 6) + ','
                    + QString::number(place["longitude"].toDouble(), 'f', 6);
            entry = place;
        }
        values.setValue("places/favorites", QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
        values.setValue("places/listVersion", 2);
    }
}
