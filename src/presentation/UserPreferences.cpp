#include "UserPreferences.hpp"
#include "ChartConfiguration.hpp"
#include "ForecastFields.hpp"
#include "ProfileFileCodec.hpp"
#include "core/Localization.hpp"
#include "core/Locations.hpp"
#include <QJsonDocument>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

QVariantMap UserPreferences::factoryProfile(const QString &id) const
{
    for (const auto &entry : m_factoryProfiles)
        if (entry.toMap()["id"] == id) return entry.toMap();
    return {};
}

UserPreferences::UserPreferences(QObject *parent, QVariantList defaults)
    : QObject(parent), m_factoryProfiles(defaults.isEmpty() ? builtinChartProfiles() : std::move(defaults))
{
    connect(&m_store, &PreferencesStore::placesChanged, this, &UserPreferences::placesChanged);
    connect(&m_store, &PreferencesStore::displayChanged, this, &UserPreferences::displayChanged);
}

QVariantList UserPreferences::readPlaces(const QString &key) const
{
    return QJsonDocument::fromJson(m_store.value(key).toByteArray()).toVariant().toList();
}
void UserPreferences::writePlaces(const QString &key, const QVariantList &places)
{
    m_store.setValue(key, QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
}
QVariantList UserPreferences::favorites() const { return readPlaces(QStringLiteral("places/favorites")); }
QVariantList UserPreferences::recentPlaces() const { return readPlaces(QStringLiteral("places/recent")); }
QString UserPreferences::profile() const
{
    const auto selected = m_store.value("display/profile", "classic").toString();
    const auto available = profiles();
    for (const auto &entry : available)
        if (entry.toMap()["id"] == selected) return selected;
    return available.isEmpty() ? QStringLiteral("classic") : available.first().toMap()["id"].toString();
}
QStringList UserPreferences::visibleGroups() const
{
    QStringList result;
    for (const auto &entry : chartLayout())
        if (entry.toMap()["enabled"].toBool())
            result.append(entry.toMap()["key"].toString());
    return result;
}
QString UserPreferences::windUnit() const { return m_store.value("display/windUnit", "m/s").toString(); }
bool UserPreferences::utc() const { return m_store.value("display/utc", false).toBool(); }
bool UserPreferences::showNight() const { return m_store.value("display/showNight", true).toBool(); }
bool UserPreferences::showLegend() const { return m_store.value("display/showLegend", false).toBool(); }
void UserPreferences::setShowLegend(bool value)
{
    change([&] {
        if (value == showLegend())
            return;
        m_store.setValue("display/showLegend", value);
    });
}
int UserPreferences::chartHeight() const
{
    return std::clamp(m_store.value("display/chartHeight", 174).toInt(), 64, 800);
}
int UserPreferences::profileChartHeight() const { return chartHeightForProfile(profile()); }
int UserPreferences::chartHeightForProfile(const QString &id) const
{
    const auto available = profiles();
    const bool listed = std::any_of(available.cbegin(), available.cend(), [&](const QVariant &entry) { return entry.toMap()["id"] == id; });
    if (!listed && !available.isEmpty()) return chartHeightForProfile(available.first().toMap()["id"].toString());
    if (id == "custom")
        return m_store.value("display/customChartHeight", 0).toInt();
    const auto height = factoryProfile(id).value("chartHeight", 0);
    for (const auto &value : savedProfiles())
        if (value.toMap()["id"] == id)
            return value.toMap().value("chartHeight", height).toInt();
    return height.toInt();
}
void UserPreferences::setChartHeight(int value)
{
    change([&] {
        value = std::clamp(value, 64, 800);
        if (value == chartHeight())
            return;
        m_store.setValue("display/chartHeight", value);
    });
}
void UserPreferences::setProfileChartHeight(int value)
{
    const auto selectedProfile = profile();
    change([&] {
        value = value ? std::clamp(value, 64, 800) : 0;
        if (value == chartHeightForProfile(selectedProfile))
            return;
        if (selectedProfile == "custom") {
            m_store.setValue("display/customChartHeight", value);
        } else saveProfileOptions(selectedProfile, {{"chartHeight", value}});
    });
}
QString UserPreferences::probeMode() const
{
    const auto mode = m_store.value("display/probeMode", "chart").toString();
    return QStringList{"chart", "all"}.contains(mode) ? mode : QStringLiteral("chart");
}
void UserPreferences::setProbeMode(const QString &value)
{
    change([&] {
        if (!QStringList{"chart", "all"}.contains(value) || value == probeMode())
            return;
        m_store.setValue("display/probeMode", value);
    });
}
QString UserPreferences::renderer() const
{
    return m_store.value("display/renderer", "gpu").toString() == "cpu" ? "cpu" : "gpu";
}
void UserPreferences::setRenderer(const QString &value)
{
    change([&] {
        if ((value != "gpu" && value != "cpu") || value == renderer())
            return;
        m_store.setValue("display/renderer", value);
    });
}
void UserPreferences::setProfile(const QString &value)
{
    change([&] {
        if (value == profile())
            return;
        bool valid = false;
        for (const auto &entry : profiles())
            if (entry.toMap()["id"] == value)
                valid = true;
        if (!valid)
            return;
        m_store.setValue("display/profile", value);
    });
}
void UserPreferences::setVisibleGroups(const QStringList &value)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &entry : layout) {
            auto chart = entry.toMap();
            chart["enabled"] = value.contains(chart["key"].toString());
            entry = chart;
        }
        saveLayout(selectedProfile, layout);
    });
}
void UserPreferences::setWindUnit(const QString &value)
{
    change([&] {
        if (!QStringList{"m/s", "km/h", "kt"}.contains(value))
            return;
        m_store.setValue("display/windUnit", value);
    });
}
void UserPreferences::setUtc(bool value)
{
    change([&] { m_store.setValue("display/utc", value); });
}
void UserPreferences::setShowNight(bool value)
{
    change([&] { m_store.setValue("display/showNight", value); });
}

QVariantList UserPreferences::savedProfiles() const
{
    return QJsonDocument::fromJson(m_store.value("display/savedProfiles").toByteArray()).toVariant().toList();
}
QVariantList UserPreferences::profiles() const
{
    QVariantList result;
    const auto saved = savedProfiles();
    const auto deleted = m_store.value("display/deletedProfiles").toStringList();
    for (const auto &value : m_factoryProfiles) {
        const auto preset = value.toMap();
        const auto id = preset["id"].toString();
        if (deleted.contains(id)) continue;
        QVariantMap override;
        for (const auto &entry : saved)
            if (entry.toMap()["id"] == id) { override = entry.toMap(); break; }
        const auto availableRevision = preset["revision"].toInt();
        const auto revision = override.isEmpty() ? availableRevision : override.value("baseRevision", 0).toInt();
        result.append(QVariantMap{{"id", id}, {"name", override.value("name", preset["name"])},
                                 {"builtin", true}, {"modified", !override.isEmpty()},
                                 {"revision", revision}, {"availableRevision", availableRevision},
                                 {"updateAvailable", !override.isEmpty() && revision < availableRevision}});
    }
    if (m_store.contains("display/customLayout"))
        result.append(QVariantMap{{"id", "custom"},
            {"name", m_store.value("display/customProfileName", Meteo::i18n("Custom layout"))}, {"builtin", false}});
    for (const auto &value : saved) {
        const auto entry = value.toMap();
        const auto id = entry["id"].toString();
        // A retired preset's edited snapshot remains usable with the same ID,
        // including in widgets that selected it independently of the main app.
        if (!id.isEmpty() && id != "custom" && factoryProfile(id).isEmpty() && !deleted.contains(id))
            result.append(QVariantMap{{"id", id}, {"name", entry["name"]}, {"builtin", false},
                {"revision", entry.value("revision", entry.value("baseRevision", 1))}});
    }
    return result;
}
bool UserPreferences::hasDeletedProfiles() const { return !m_store.value("display/deletedProfiles").toStringList().isEmpty(); }
QVariantList UserPreferences::chartLayout() const { return chartLayoutForProfile(profile()); }
QVariantList UserPreferences::chartLayoutForProfile(const QString &id) const
{
    const auto available = profiles();
    const bool listed = std::any_of(available.cbegin(), available.cend(), [&](const QVariant &entry) { return entry.toMap()["id"] == id; });
    if (!listed && !available.isEmpty()) return chartLayoutForProfile(available.first().toMap()["id"].toString());
    if (id == "custom") {
        const auto saved = m_store.value("display/customLayout").toByteArray();
        return saved.isEmpty() ? m_factoryProfiles.first().toMap()["charts"].toList()
                               : normalizeChartLayout(QJsonDocument::fromJson(saved).toVariant().toList());
    }
    for (const auto &value : savedProfiles()) {
        const auto entry = value.toMap();
        if (entry["id"] == id && entry.contains("charts")) return normalizeChartLayout(entry["charts"].toList());
    }
    return factoryProfile(id).value("charts", m_factoryProfiles.first().toMap()["charts"]).toList();
}
QVariantList UserPreferences::chartCatalog() const { return defaultChartCatalog(); }
QVariantMap UserPreferences::chartAppearance() const { return defaultChartAppearance(); }
QVariantList UserPreferences::seriesCatalog() const { return chartSeriesCatalog(); }
QString UserPreferences::settingsFile() const { return m_store.fileName(); }

QVariantMap UserPreferences::exportProfiles(const QUrl &url, bool allSaved) const
{
    QVariantList exported;
    if (allSaved) {
        for (const auto &value : savedProfiles()) {
            const auto entry = value.toMap();
            const auto id = entry["id"].toString();
            const auto source = factoryProfile(id);
            const int revision = source.isEmpty() ? entry.value("revision", entry.value("baseRevision", 1)).toInt()
                : entry.value("baseRevision", source["revision"]).toInt();
            exported.append(QVariantMap{{"name", entry.value("name", factoryProfile(id)["name"])},
                                        {"charts", chartLayoutForProfile(id)},
                                        {"revision", std::max(1, revision)},
                                        {"chartHeight", entry.value("chartHeight", 0)}});
        }
        if (m_store.contains("display/customLayout"))
            exported.append(QVariantMap{
                {"name", Meteo::i18n("Custom layout")},
                {"charts",
                 normalizeChartLayout(QJsonDocument::fromJson(m_store.value("display/customLayout").toByteArray())
                                          .toVariant()
                                          .toList())},
                {"chartHeight", m_store.value("display/customChartHeight", 0)}});
    }
    if (exported.isEmpty()) {
        QString name;
        int revision = 1;
        for (const auto &value : profiles())
            if (value.toMap()["id"] == profile()) {
                name = value.toMap()["name"].toString();
                revision = std::max(1, value.toMap().value("revision", 1).toInt());
            }
        exported.append(QVariantMap{{"name", name}, {"charts", chartLayout()}, {"chartHeight", profileChartHeight()}, {"revision", revision}});
    }
    QString error;
    if (!ProfileFileCodec::write(url, exported, &error))
        return {{"ok", false}, {"error", error}};
    return {{"ok", true}, {"count", exported.size()}, {"path", url.toLocalFile()}};
}

QVariantMap UserPreferences::importProfiles(const QUrl &url)
{
    QString error;
    const auto imported = ProfileFileCodec::read(url, &error);
    if (!imported)
        return {{"ok", false}, {"error", error}};
    const bool persisted = change([&] {
        auto saved = savedProfiles();
        QSet<QString> names;
        for (const auto &value : profiles())
            names.insert(value.toMap()["name"].toString());
        QString selected;
        for (const auto &value : *imported) {
            const auto entry = value.toMap();
            const auto baseName = entry["name"].toString();
            QString name = baseName;
            for (int suffix = 2; names.contains(name); ++suffix)
                name = baseName.left(70) + " (" + QString::number(suffix) + ")";
            names.insert(name);
            const auto id = "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            saved.append(QVariantMap{
                {"id", id}, {"name", name}, {"charts", entry["charts"]}, {"chartHeight", entry["chartHeight"]}, {"revision", entry.value("revision", 1)}});
            if (selected.isEmpty())
                selected = id;
        }
        m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        m_store.setValue("display/profile", selected);
    });
    if (!persisted)
        return {{"ok", false}, {"error", m_error}};
    return {{"ok", true}, {"count", imported->size()}};
}
void UserPreferences::saveLayout(const QString &id, const QVariantList &layout)
{
    const auto normalized = normalizeChartLayout(layout);
    if (normalized == chartLayoutForProfile(id))
        return;
    if (id == "custom") {
        m_store.setValue("display/customLayout", QJsonDocument::fromVariant(normalized).toJson(QJsonDocument::Compact));
    } else saveProfileOptions(id, {{"charts", normalized}});
}
void UserPreferences::saveProfileOptions(const QString &id, const QVariantMap &options)
{
    auto saved = savedProfiles();
    for (auto &value : saved) {
        auto entry = value.toMap();
        if (entry["id"] != id) continue;
        for (auto it = options.cbegin(); it != options.cend(); ++it) entry[it.key()] = it.value();
        value = entry;
        m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        return;
    }
    auto entry = factoryProfile(id);
    if (entry.isEmpty() || m_store.value("display/deletedProfiles").toStringList().contains(id)) return;
    entry["baseRevision"] = entry.take("revision");
    for (auto it = options.cbegin(); it != options.cend(); ++it) entry[it.key()] = it.value();
    saved.append(entry);
    m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
}
void UserPreferences::setChartEnabled(const QString &key, bool enabled)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &entry : layout) {
            auto chart = entry.toMap();
            if (chart["key"] == key) {
                chart["enabled"] = enabled;
                entry = chart;
                break;
            }
        }
        saveLayout(selectedProfile, layout);
    });
}
void UserPreferences::setChartField(const QString &key, const QString &field, bool enabled)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &entry : layout) {
            auto chart = entry.toMap();
            if (chart["key"] != key)
                continue;
            auto series = chart["series"].toList();
            bool found = false;
            for (auto &item : series) {
                auto style = item.toMap();
                if (style["key"] != field)
                    continue;
                style["enabled"] = enabled;
                item = style;
                found = true;
                break;
            }
            if (!found && enabled) {
                const auto style = defaultSeriesStyle(field);
                if (!style.isEmpty())
                    series.append(style);
            }
            chart["series"] = series;
            entry = chart;
            break;
        }
        saveLayout(selectedProfile, normalizeChartLayout(layout));
    });
}
QString UserPreferences::createChart(const QString &title)
{
    const auto selectedProfile = profile();
    QString result;
    const bool saved = change([&] {
        result = [&]() -> QString {
            auto layout = chartLayoutForProfile(selectedProfile);
            if (layout.size() >= 50)
                return {};
            const auto key = "chart:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            auto chart = defaultChartPrototype();
            chart["key"] = key;
            if (!title.trimmed().isEmpty()) chart["title"] = title.trimmed().left(80);
            layout.append(chart);
            saveLayout(selectedProfile, normalizeChartLayout(layout));
            return key;
        }();
    });
    return saved ? result : QString{};
}
void UserPreferences::deleteChart(const QString &key)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        layout.removeIf([&key](const QVariant &value) { return value.toMap()["key"] == key; });
        saveLayout(selectedProfile, layout);
    });
}
void UserPreferences::addChart(const QString &key)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &entry : layout) {
            auto chart = entry.toMap();
            if (chart["key"] != key) continue;
            chart["enabled"] = true;
            entry = chart;
            saveLayout(selectedProfile, layout);
            return;
        }
        if (layout.size() >= 50) return;
        for (const auto &entry : defaultChartCatalog())
            if (entry.toMap()["key"] == key) {
                layout.append(entry);
                saveLayout(selectedProfile, layout);
                return;
            }
    });
}
void UserPreferences::setChartOptions(const QString &key, const QVariantMap &options)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &value : layout) {
            auto chart = value.toMap();
            if (chart["key"] != key)
                continue;
            for (auto it = options.cbegin(); it != options.cend(); ++it)
                if (QStringList{"title", "leftMin", "leftMax", "rightMin", "rightMax", "leftScale", "rightScale",
                                "height"}
                        .contains(it.key()))
                    chart[it.key()] = it.value();
            value = chart;
            break;
        }
        saveLayout(selectedProfile, normalizeChartLayout(layout));
    });
}
void UserPreferences::setSeriesStyle(const QString &key, const QString &field, const QVariantMap &options)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &value : layout) {
            auto chart = value.toMap();
            if (chart["key"] != key)
                continue;
            auto series = chart["series"].toList();
            for (auto &entry : series) {
                auto style = entry.toMap();
                if (style["key"] != field)
                    continue;
                for (auto it = options.cbegin(); it != options.cend(); ++it)
                    if (it.key() != "key")
                        style[it.key()] = it.value();
                entry = normalizeSeriesStyle(style);
                break;
            }
            chart["series"] = series;
            value = chart;
            break;
        }
        saveLayout(selectedProfile, normalizeChartLayout(layout));
    });
}
void UserPreferences::removeSeries(const QString &key, const QString &field)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        for (auto &value : layout) {
            auto chart = value.toMap();
            if (chart["key"] != key)
                continue;
            auto series = chart["series"].toList();
            series.removeIf([&field](const QVariant &entry) { return entry.toMap()["key"] == field; });
            chart["series"] = series;
            value = chart;
            break;
        }
        saveLayout(selectedProfile, normalizeChartLayout(layout));
    });
}
void UserPreferences::moveChart(int index, int direction)
{
    const auto selectedProfile = profile();
    change([&] {
        auto layout = chartLayoutForProfile(selectedProfile);
        const auto next = index + direction;
        if (index < 0 || index >= layout.size() || next < 0 || next >= layout.size())
            return;
        layout.swapItemsAt(index, next);
        saveLayout(selectedProfile, layout);
    });
}
QString UserPreferences::createProfileFrom(const QString &name, const QString &sourceProfile)
{
    QString result;
    const bool saved = change([&] {
        result = [&]() -> QString {
            if (name.trimmed().isEmpty())
                return {};
            const auto id = "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            auto saved = savedProfiles();
            saved.append(QVariantMap{{"id", id},
                                     {"name", name.trimmed().left(80)},
                                     {"charts", chartLayoutForProfile(sourceProfile)},
                                     {"chartHeight", chartHeightForProfile(sourceProfile)}});
            m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
            m_store.setValue("display/profile", id);
            return id;
        }();
    });
    return saved ? result : QString{};
}
QString UserPreferences::duplicateProfile()
{
    const auto selectedProfile = profile();
    QString result;
    const bool saved = change([&] {
        result = [&]() -> QString {
            QString base;
            QStringList names;
            for (const auto &value : profiles()) {
                const auto entry = value.toMap();
                names.append(entry["name"].toString());
                if (entry["id"] == selectedProfile)
                    base = entry["name"].toString();
            }
            base = Meteo::i18n("%1 — copy").arg(base.left(60));
            auto name = base;
            for (int number = 2; names.contains(name, Qt::CaseInsensitive); ++number)
                name = base + QStringLiteral(" (%1)").arg(number);
            return createProfileFrom(name, selectedProfile);
        }();
    });
    return saved ? result : QString{};
}
void UserPreferences::renameProfile(const QString &name)
{
    const auto selectedProfile = profile();
    change([&] {
        if (name.trimmed().isEmpty()) return;
        if (selectedProfile == "custom") m_store.setValue("display/customProfileName", name.trimmed().left(80));
        else saveProfileOptions(selectedProfile, {{"name", name.trimmed().left(80)}});
    });
}
void UserPreferences::deleteProfile()
{
    const auto selectedProfile = profile();
    change([&] {
        if (profiles().size() <= 1) return;
        const bool wasSelected = profile() == selectedProfile;
        if (selectedProfile == "custom") {
            m_store.remove("display/customLayout");
            m_store.remove("display/customChartHeight");
            m_store.remove("display/customProfileName");
        } else if (!factoryProfile(selectedProfile).isEmpty()) {
            auto deleted = m_store.value("display/deletedProfiles").toStringList();
            if (!deleted.contains(selectedProfile)) deleted.append(selectedProfile);
            m_store.setValue("display/deletedProfiles", deleted);
        }
        auto saved = savedProfiles();
        saved.removeIf([&](const QVariant &item) { return item.toMap()["id"] == selectedProfile; });
        m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        if (wasSelected) m_store.setValue("display/profile", profiles().first().toMap()["id"]);
    });
}
void UserPreferences::resetProfile()
{
    const auto selectedProfile = profile();
    change([&] {
        if (factoryProfile(selectedProfile).isEmpty()) return;
        auto saved = savedProfiles();
        for (const auto &entry : saved)
            if (entry.toMap()["id"] == selectedProfile
                && entry.toMap().value("baseRevision", 0).toInt() < factoryProfile(selectedProfile)["revision"].toInt()) {
                updateProfile(selectedProfile);
                return;
            }
        saved.removeIf([&](const QVariant &item) { return item.toMap()["id"] == selectedProfile; });
        m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    });
}
void UserPreferences::restoreDeletedProfiles()
{
    change([&] { m_store.remove("display/deletedProfiles"); });
}
void UserPreferences::updateProfile(const QString &id)
{
    const auto selectedProfile = id.isEmpty() ? profile() : id;
    change([&] {
        const auto factory = factoryProfile(selectedProfile);
        if (factory.isEmpty()) return;
        auto saved = savedProfiles();
        for (qsizetype i = 0; i < saved.size(); ++i) {
            const auto old = saved[i].toMap();
            if (old["id"] != selectedProfile || old.value("baseRevision", 0).toInt() >= factory["revision"].toInt()) continue;
            QStringList names;
            for (const auto &entry : profiles()) names.append(entry.toMap()["name"].toString());
            const auto base = Meteo::i18n("%1 (old)").arg(old.value("name", factory["name"]).toString().left(65));
            auto name = base;
            for (int number = 2; names.contains(name, Qt::CaseInsensitive); ++number) name = base + QStringLiteral(" (%1)").arg(number);
            saved.removeAt(i);
            saved.append(QVariantMap{{"id", "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces)},
                {"name", name}, {"charts", old.value("charts", factory["charts"])},
                {"revision", std::max(1, old.value("baseRevision", 1).toInt())},
                {"chartHeight", old.value("chartHeight", factory["chartHeight"])}});
            m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
            return;
        }
    });
}
bool UserPreferences::isFavorite(double latitude, double longitude) const
{
    return !placeAt(latitude, longitude).isEmpty();
}
QVariantMap UserPreferences::placeAt(double latitude, double longitude) const
{
    for (const auto &value : favorites()) {
        const auto place = value.toMap();
        if (Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), latitude, longitude))
            return place;
    }
    return {};
}
void UserPreferences::savePlace(const QString &name, double latitude, double longitude)
{
    // Explicit saves are upserts. For the same coordinates, the last committed
    // save/remove wins; changes to other places are preserved under the lock.
    change([&] {
        if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
            std::abs(latitude) > 90 || std::abs(longitude) > 180)
            return;
        auto places = favorites();
        for (auto &entry : places) {
            auto place = entry.toMap();
            if (!Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), latitude, longitude))
                continue;
            if (place["name"].toString() == name.trimmed())
                return;
            place["name"] = name.trimmed();
            entry = place;
            writePlaces("places/favorites", places);
            return;
        }
        places.append(QVariantMap{{"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
        writePlaces("places/favorites", places);
    });
}
void UserPreferences::removePlace(double latitude, double longitude)
{
    change([&] {
        auto places = favorites();
        const auto count = places.size();
        places.removeIf([&](const QVariant &entry) {
            const auto place = entry.toMap();
            return Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), latitude, longitude);
        });
        if (places.size() != count)
            writePlaces("places/favorites", places);
    });
}
void UserPreferences::rememberPlace(const QString &name, double latitude, double longitude)
{
    change([&] {
        if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
            std::abs(latitude) > 90 || std::abs(longitude) > 180)
            return;
        savePlace(name, latitude, longitude);
        auto places = recentPlaces();
        places.removeIf([latitude, longitude](const QVariant &v) {
            const auto p = v.toMap();
            return Meteo::samePlace(p["latitude"].toDouble(), p["longitude"].toDouble(), latitude, longitude);
        });
        places.prepend(QVariantMap{{"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
        while (places.size() > 12)
            places.removeLast();
        writePlaces("places/recent", places);
    });
}
void UserPreferences::toggleFavorite(const QString &name, double latitude, double longitude)
{
    change([&] {
        if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
            std::abs(latitude) > 90 || std::abs(longitude) > 180)
            return;
        auto places = favorites();
        const bool removed = places.removeIf([latitude, longitude](const QVariant &v) {
            const auto p = v.toMap();
            return Meteo::samePlace(p["latitude"].toDouble(), p["longitude"].toDouble(), latitude, longitude);
        }) > 0;
        if (!removed)
            places.append(QVariantMap{{"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
        writePlaces("places/favorites", places);
    });
}

bool UserPreferences::change(const std::function<void()> &operation)
{
    const bool nested = m_store.updating();
    QString error;
    const bool saved = m_store.update(operation, &error);
    if (!nested) {
        const auto message = saved ? QString() : Meteo::i18n("Could not save preferences: %1").arg(error);
        if (message != m_error) { m_error = message; emit errorChanged(); }
        // Re-evaluate edited controls against the last committed values.
        if (!saved) emit displayChanged();
    }
    return saved;
}

QString UserPreferences::createProfile(const QString &name) { return createProfileFrom(name, profile()); }

QStringList UserPreferences::seriesRenderers(const QString &key) const { return ::seriesRenderers(key); }
