#include "core/Localization.hpp"
#include "UserPreferences.hpp"
#include "ForecastFields.hpp"
#include "ChartConfiguration.hpp"
#include "ProfileFileCodec.hpp"
#include <QJsonDocument>
#include <QSet>
#include <cmath>
#include <QUuid>
#include <algorithm>

UserPreferences::UserPreferences(QObject *parent) : QObject(parent)
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
    m_store.sync();
}
QVariantList UserPreferences::favorites() const { return readPlaces(QStringLiteral("places/favorites")); }
QVariantList UserPreferences::recentPlaces() const { return readPlaces(QStringLiteral("places/recent")); }
QString UserPreferences::profile() const { return m_store.value("display/profile", "classic").toString(); }
QStringList UserPreferences::visibleGroups() const
{
    QStringList result;
    for (const auto &entry : chartLayout()) if (entry.toMap()["enabled"].toBool()) result.append(entry.toMap()["key"].toString());
    return result;
}
QString UserPreferences::windUnit() const { return m_store.value("display/windUnit", "m/s").toString(); }
bool UserPreferences::utc() const { return m_store.value("display/utc", false).toBool(); }
bool UserPreferences::showNight() const { return m_store.value("display/showNight", true).toBool(); }
bool UserPreferences::showLegend() const { return m_store.value("display/showLegend", true).toBool(); }
void UserPreferences::setShowLegend(bool value) { if (value==showLegend()) return; m_store.setValue("display/showLegend",value);m_store.sync(); }
int UserPreferences::chartHeight() const { return std::clamp(m_store.value("display/chartHeight", 174).toInt(),64,800); }
int UserPreferences::profileChartHeight() const
{
    return chartHeightForProfile(profile());
}
int UserPreferences::chartHeightForProfile(const QString &id) const
{
    if (id == "custom") return m_store.value("display/customChartHeight", 0).toInt();
    for (const auto &value : savedProfiles()) if (value.toMap()["id"] == id) return value.toMap().value("chartHeight",0).toInt();
    return 0;
}
void UserPreferences::setChartHeight(int value)
{
    value=std::clamp(value,64,800);
    if (value==chartHeight()) return;
    m_store.setValue("display/chartHeight",value);m_store.sync();
}
void UserPreferences::setProfileChartHeight(int value)
{
    value=value ? std::clamp(value,64,800) : 0;
    if (value==profileChartHeight()) return;
    if (profile().startsWith("user:")) {
        auto saved=savedProfiles();
        for (auto &item:saved) { auto entry=item.toMap();if (entry["id"]==profile()) { entry["chartHeight"]=value;item=entry;break; } }
        m_store.setValue("display/savedProfiles",QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    } else {
        if (profile()!="custom") m_store.setValue("display/customLayout",QJsonDocument::fromVariant(chartLayout()).toJson(QJsonDocument::Compact));
        m_store.setValue("display/profile","custom");m_store.setValue("display/customChartHeight",value);
    }
    m_store.sync();
}
QString UserPreferences::probeMode() const
{
    const auto mode = m_store.value("display/probeMode", "chart").toString();
    return QStringList{"chart", "all"}.contains(mode) ? mode : QStringLiteral("chart");
}
void UserPreferences::setProbeMode(const QString &value)
{
    if (!QStringList{"chart", "all"}.contains(value) || value == probeMode()) return;
    m_store.setValue("display/probeMode", value); m_store.sync();
}
QString UserPreferences::renderer() const { return m_store.value("display/renderer", "gpu").toString() == "cpu" ? "cpu" : "gpu"; }
void UserPreferences::setRenderer(const QString &value)
{
    if ((value != "gpu" && value != "cpu") || value == renderer()) return;
    m_store.setValue("display/renderer", value); m_store.sync();
}
void UserPreferences::setProfile(const QString &value)
{
    if (value == profile()) return;
    bool valid = false;
    for (const auto &entry : profiles()) if (entry.toMap()["id"] == value) valid = true;
    if (!valid) return;
    m_store.setValue("display/profile", value); m_store.sync();
}
void UserPreferences::setVisibleGroups(const QStringList &value)
{
    auto layout = chartLayout();
    for (auto &entry : layout) { auto chart = entry.toMap(); chart["enabled"] = value.contains(chart["key"].toString()); entry = chart; }
    saveLayout(layout);
}
void UserPreferences::setWindUnit(const QString &value)
{
    if (!QStringList{"m/s", "km/h", "kt"}.contains(value)) return;
    m_store.setValue("display/windUnit", value); m_store.sync();
}
void UserPreferences::setUtc(bool value) { m_store.setValue("display/utc", value); m_store.sync();  }
void UserPreferences::setShowNight(bool value) { m_store.setValue("display/showNight", value); m_store.sync();  }

QVariantList UserPreferences::savedProfiles() const
{
    return QJsonDocument::fromJson(m_store.value("display/savedProfiles").toByteArray()).toVariant().toList();
}
QVariantList UserPreferences::profiles() const
{
    QVariantList result;
    const QList<QPair<QString, QString>> presets{{"classic", Meteo::i18n("Classic ICM")}, {"full", Meteo::i18n("All charts")}, {"basic", Meteo::i18n("Basic")}, {"aviation", Meteo::i18n("Aviation")}, {"outdoor", Meteo::i18n("Outdoors")}, {"custom", Meteo::i18n("Custom layout")}};
    for (const auto &preset : presets) result.append(QVariantMap{{"id", preset.first}, {"name", preset.second}, {"builtin", true}});
    for (const auto &entry : savedProfiles()) result.append(QVariantMap{{"id", entry.toMap()["id"]}, {"name", entry.toMap()["name"]}, {"builtin", false}});
    return result;
}
QVariantList UserPreferences::chartLayout() const
{
    return chartLayoutForProfile(profile());
}
QVariantList UserPreferences::chartLayoutForProfile(const QString &id) const
{
    if (id == "custom") {
        const auto saved = m_store.value("display/customLayout").toByteArray();
        return saved.isEmpty() ? defaultChartLayout("classic") : normalizeChartLayout(QJsonDocument::fromJson(saved).toVariant().toList());
    }
    if (id.startsWith("user:")) for (const auto &entry : savedProfiles())
        if (entry.toMap()["id"] == id) return normalizeChartLayout(entry.toMap()["charts"].toList());
    return defaultChartLayout(id);
}
QVariantList UserPreferences::chartCatalog() const
{
    return defaultChartLayout("full");
}
QVariantList UserPreferences::seriesCatalog() const { return chartSeriesCatalog(); }
QString UserPreferences::settingsFile() const { return m_store.fileName(); }

QVariantMap UserPreferences::exportProfiles(const QUrl &url, bool allSaved) const
{
    QVariantList exported;
    if (allSaved) {
        for (const auto &value : savedProfiles()) {
            const auto entry = value.toMap();
            exported.append(QVariantMap{{"name", entry["name"]}, {"charts", normalizeChartLayout(entry["charts"].toList())}, {"chartHeight",entry.value("chartHeight",0)}});
        }
        if (m_store.contains("display/customLayout")) exported.append(QVariantMap{{"name", Meteo::i18n("Custom layout")},
            {"charts", normalizeChartLayout(QJsonDocument::fromJson(m_store.value("display/customLayout").toByteArray()).toVariant().toList())}, {"chartHeight",m_store.value("display/customChartHeight",0)}});
    }
    if (exported.isEmpty()) {
        QString name;
        for (const auto &value : profiles()) if (value.toMap()["id"] == profile()) name = value.toMap()["name"].toString();
        exported.append(QVariantMap{{"name", name}, {"charts", chartLayout()}, {"chartHeight",profileChartHeight()}});
    }
    QString error;
    if (!ProfileFileCodec::write(url, exported, &error)) return {{"ok", false}, {"error", error}};
    return {{"ok", true}, {"count", exported.size()}, {"path", url.toLocalFile()}};
}

QVariantMap UserPreferences::importProfiles(const QUrl &url)
{
    QString error;
    const auto imported = ProfileFileCodec::read(url, &error);
    if (!imported) return {{"ok", false}, {"error", error}};
    auto saved = savedProfiles();
    QSet<QString> names;
    for (const auto &value : profiles()) names.insert(value.toMap()["name"].toString());
    QString selected;
    for (const auto &value : *imported) {
        const auto entry = value.toMap();
        const auto baseName = entry["name"].toString();
        QString name = baseName;
        for (int suffix = 2; names.contains(name); ++suffix) name = baseName.left(70) + " (" + QString::number(suffix) + ")";
        names.insert(name);
        const auto id = "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        saved.append(QVariantMap{{"id", id}, {"name", name}, {"charts", entry["charts"]}, {"chartHeight", entry["chartHeight"]}});
        if (selected.isEmpty()) selected = id;
    }
    m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_store.setValue("display/profile", selected);
    m_store.sync();

    return {{"ok", true}, {"count", imported->size()}};
}
void UserPreferences::saveLayout(const QVariantList &layout)
{
    const auto normalized = normalizeChartLayout(layout);
    if (normalized == chartLayout()) return;
    if (profile().startsWith("user:")) {
        auto saved = savedProfiles();
        for (auto &item : saved) { auto entry = item.toMap(); if (entry["id"] == profile()) { entry["charts"] = normalized; item = entry; break; } }
        m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    } else {
        if (profile()!="custom") m_store.setValue("display/customChartHeight",profileChartHeight());
        m_store.setValue("display/customLayout", QJsonDocument::fromVariant(normalized).toJson(QJsonDocument::Compact));
        m_store.setValue("display/profile", "custom");
    }
    m_store.sync();
}
void UserPreferences::setChartEnabled(const QString &key, bool enabled)
{
    auto layout = chartLayout();
    for (auto &entry : layout) { auto chart = entry.toMap(); if (chart["key"] == key) { chart["enabled"] = enabled; entry = chart; break; } }
    saveLayout(layout);
}
void UserPreferences::setChartField(const QString &key, const QString &field, bool enabled)
{
    auto layout = chartLayout();
    for (auto &entry : layout) {
        auto chart = entry.toMap();
        if (chart["key"] != key) continue;
        auto series = chart["series"].toList();
        bool found = false;
        for (auto &item : series) {
            auto style = item.toMap();
            if (style["key"] != field) continue;
            style["enabled"] = enabled; item = style; found = true; break;
        }
        if (!found && enabled) { const auto style = defaultSeriesStyle(field); if (!style.isEmpty()) series.append(style); }
        chart["series"] = series; entry = chart; break;
    }
    saveLayout(normalizeChartLayout(layout));
}
QString UserPreferences::createChart(const QString &title)
{
    auto layout = chartLayout();
    if (layout.size() >= 50) return {};
    const auto key = "chart:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    layout.append(QVariantMap{{"key", key}, {"title", title.trimmed().isEmpty() ? Meteo::i18n("New chart") : title.trimmed().left(80)},
        {"enabled", true}, {"leftScale", "auto"}, {"rightScale", "auto"}, {"series", QVariantList{}}});
    saveLayout(normalizeChartLayout(layout)); return key;
}
void UserPreferences::deleteChart(const QString &key)
{
    auto layout = chartLayout();
    layout.removeIf([&key](const QVariant &value) { return value.toMap()["key"] == key; });
    saveLayout(layout);
}
void UserPreferences::setChartOptions(const QString &key, const QVariantMap &options)
{
    auto layout = chartLayout();
    for (auto &value : layout) {
        auto chart = value.toMap(); if (chart["key"] != key) continue;
        for (auto it = options.cbegin(); it != options.cend(); ++it)
            if (QStringList{"title", "leftMin", "leftMax", "rightMin", "rightMax", "leftScale", "rightScale", "height"}.contains(it.key())) chart[it.key()] = it.value();
        value = chart; break;
    }
    saveLayout(normalizeChartLayout(layout));
}
void UserPreferences::setSeriesStyle(const QString &key, const QString &field, const QVariantMap &options)
{
    auto layout = chartLayout();
    for (auto &value : layout) {
        auto chart = value.toMap(); if (chart["key"] != key) continue;
        auto series = chart["series"].toList();
        for (auto &entry : series) {
            auto style = entry.toMap(); if (style["key"] != field) continue;
            for (auto it = options.cbegin(); it != options.cend(); ++it) if (it.key() != "key") style[it.key()] = it.value();
            entry = normalizeSeriesStyle(style); break;
        }
        chart["series"] = series; value = chart; break;
    }
    saveLayout(normalizeChartLayout(layout));
}
void UserPreferences::removeSeries(const QString &key, const QString &field)
{
    auto layout = chartLayout();
    for (auto &value : layout) {
        auto chart = value.toMap(); if (chart["key"] != key) continue;
        auto series = chart["series"].toList();
        series.removeIf([&field](const QVariant &entry) { return entry.toMap()["key"] == field; });
        chart["series"] = series; value = chart; break;
    }
    saveLayout(normalizeChartLayout(layout));
}
void UserPreferences::moveChart(int index, int direction)
{
    auto layout = chartLayout();
    const auto next = index + direction;
    if (index < 0 || index >= layout.size() || next < 0 || next >= layout.size()) return;
    layout.swapItemsAt(index, next); saveLayout(layout);
}
QString UserPreferences::createProfile(const QString &name)
{
    if (name.trimmed().isEmpty()) return {};
    const auto id = "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto saved = savedProfiles();
    saved.append(QVariantMap{{"id", id}, {"name", name.trimmed().left(80)}, {"charts", chartLayout()}, {"chartHeight",profileChartHeight()}});
    m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_store.setValue("display/profile", id); m_store.sync();  return id;
}
QString UserPreferences::duplicateProfile()
{
    QString base;
    QStringList names;
    for (const auto &value : profiles()) {
        const auto entry = value.toMap();
        names.append(entry["name"].toString());
        if (entry["id"] == profile()) base = entry["name"].toString();
    }
    base = Meteo::i18n("%1 — copy").arg(base.left(60));
    auto name = base;
    for (int number = 2; names.contains(name, Qt::CaseInsensitive); ++number)
        name = base + QStringLiteral(" (%1)").arg(number);
    return createProfile(name);
}
void UserPreferences::renameProfile(const QString &name)
{
    if (!profile().startsWith("user:") || name.trimmed().isEmpty()) return;
    auto saved = savedProfiles();
    for (auto &item : saved) { auto entry = item.toMap(); if (entry["id"] == profile()) { entry["name"] = name.trimmed().left(80); item = entry; break; } }
    m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_store.sync();
}
void UserPreferences::deleteProfile()
{
    if (profile() == "custom") {
        m_store.remove("display/customLayout");
        m_store.remove("display/customChartHeight");
        m_store.setValue("display/profile", "classic"); m_store.sync();
        return;
    }
    if (!profile().startsWith("user:")) return;
    auto saved = savedProfiles();
    saved.removeIf([this](const QVariant &item) { return item.toMap()["id"] == profile(); });
    m_store.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_store.setValue("display/profile", "classic"); m_store.sync();
}
bool UserPreferences::isFavorite(double latitude, double longitude) const
{
    for (const auto &value : favorites()) {
        const auto place = value.toMap();
        if (std::abs(place["latitude"].toDouble() - latitude) < 0.000001 && std::abs(place["longitude"].toDouble() - longitude) < 0.000001) return true;
    }
    return false;
}
void UserPreferences::savePlace(const QString &name, double latitude, double longitude)
{
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180) return;
    auto places = favorites();
    for (auto &entry : places) {
        auto place = entry.toMap();
        if (std::abs(place["latitude"].toDouble()-latitude) >= 0.000001 || std::abs(place["longitude"].toDouble()-longitude) >= 0.000001) continue;
        if (place["name"].toString() == name.trimmed()) return;
        place["name"] = name.trimmed(); entry = place;
        writePlaces("places/favorites", places); return;
    }
    places.append(QVariantMap{{"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
    writePlaces("places/favorites", places);
}
void UserPreferences::removePlace(double latitude, double longitude)
{
    auto places = favorites();
    const auto count = places.size();
    places.removeIf([&](const QVariant &entry) {
        const auto place = entry.toMap();
        return std::abs(place["latitude"].toDouble()-latitude) < 0.000001 && std::abs(place["longitude"].toDouble()-longitude) < 0.000001;
    });
    if (places.size() != count) writePlaces("places/favorites", places);
}
void UserPreferences::rememberPlace(const QString &name, double latitude, double longitude)
{
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180) return;
    savePlace(name, latitude, longitude);
    auto places = recentPlaces();
    places.removeIf([latitude, longitude](const QVariant &v) {
        const auto p = v.toMap();
        return std::abs(p["latitude"].toDouble() - latitude) < 0.000001 && std::abs(p["longitude"].toDouble() - longitude) < 0.000001;
    });
    places.prepend(QVariantMap{{"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
    while (places.size() > 12) places.removeLast();
    writePlaces("places/recent", places);
}
void UserPreferences::toggleFavorite(const QString &name, double latitude, double longitude)
{
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180) return;
    auto places = favorites();
    const bool removed = places.removeIf([latitude, longitude](const QVariant &v) {
        const auto p = v.toMap();
        return std::abs(p["latitude"].toDouble() - latitude) < 0.000001 && std::abs(p["longitude"].toDouble() - longitude) < 0.000001;
    }) > 0;
    if (!removed) places.append(QVariantMap{{"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
    writePlaces("places/favorites", places);
}
