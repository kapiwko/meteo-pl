#include "UserPreferences.hpp"
#include "ForecastFields.hpp"
#include <QJsonDocument>
#include <QSet>
#include <cmath>
#include <QUuid>

namespace {
QVariantList defaultLayout(const QString &profile)
{
    QStringList enabled{"temperature", "precipitation", "probability", "pressure", "wind", "windDirection", "cloudHeights", "clouds"};
    if (profile == "basic") enabled = {"temperature", "precipitation", "wind", "pressure", "clouds"};
    if (profile == "outdoor") enabled = {"temperature", "precipitation", "probability", "wind", "clouds"};
    QVariantList layout;
    for (const auto &chart : forecastCharts())
        layout.append(QVariantMap{{"key", chart.key}, {"enabled", profile == "full" || enabled.contains(chart.key)}, {"fields", chart.fields}});
    return layout;
}
QVariantList normalizeLayout(const QVariantList &saved)
{
    const auto defaults = defaultLayout("classic");
    QVariantList result;
    QSet<QString> seen;
    for (const auto &item : saved) {
        auto entry = item.toMap();
        const auto key = entry["key"].toString();
        if (seen.contains(key)) continue;
        for (const auto &standard : defaults) {
            const auto chart = standard.toMap();
            if (chart["key"] != key) continue;
            auto fields = entry.value("fields", chart["fields"]).toStringList();
            const auto allowed = chart["fields"].toStringList();
            fields.removeIf([&allowed](const QString &field) { return !allowed.contains(field); });
            result.append(QVariantMap{{"key", key}, {"enabled", entry["enabled"].toBool()}, {"fields", fields}});
            seen.insert(key);
            break;
        }
    }
    for (const auto &standard : defaults) {
        auto entry = standard.toMap();
        if (seen.contains(entry["key"].toString())) continue;
        entry["enabled"] = false;
        result.append(entry);
    }
    return result;
}
}

UserPreferences::UserPreferences(QObject *parent)
    : QObject(parent), m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("meteo-pl"), QStringLiteral("desktop"))
{
    if (m_settings.value("display/layoutVersion", 0).toInt() < 2) {
        m_settings.setValue("display/profile", "classic");
        m_settings.setValue("display/layoutVersion", 2);
        m_settings.sync();
    }
}

QVariantList UserPreferences::readPlaces(const QString &key) const
{
    m_settings.sync();
    return QJsonDocument::fromJson(m_settings.value(key).toByteArray()).toVariant().toList();
}
void UserPreferences::writePlaces(const QString &key, const QVariantList &places)
{
    m_settings.setValue(key, QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
    m_settings.sync();
    emit placesChanged();
}
QVariantList UserPreferences::favorites() const { return readPlaces(QStringLiteral("places/favorites")); }
QVariantList UserPreferences::recentPlaces() const { return readPlaces(QStringLiteral("places/recent")); }
QString UserPreferences::profile() const { return m_settings.value("display/profile", "classic").toString(); }
QStringList UserPreferences::visibleGroups() const
{
    QStringList result;
    for (const auto &entry : chartLayout()) if (entry.toMap()["enabled"].toBool()) result.append(entry.toMap()["key"].toString());
    return result;
}
QString UserPreferences::windUnit() const { return m_settings.value("display/windUnit", "m/s").toString(); }
bool UserPreferences::utc() const { return m_settings.value("display/utc", false).toBool(); }
bool UserPreferences::showNight() const { return m_settings.value("display/showNight", true).toBool(); }
void UserPreferences::setProfile(const QString &value)
{
    if (value == profile()) return;
    bool valid = false;
    for (const auto &entry : profiles()) if (entry.toMap()["id"] == value) valid = true;
    if (!valid) return;
    m_settings.setValue("display/profile", value); m_settings.sync(); emit displayChanged();
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
    m_settings.setValue("display/windUnit", value); m_settings.sync(); emit displayChanged();
}
void UserPreferences::setUtc(bool value) { m_settings.setValue("display/utc", value); m_settings.sync(); emit displayChanged(); }
void UserPreferences::setShowNight(bool value) { m_settings.setValue("display/showNight", value); m_settings.sync(); emit displayChanged(); }

QVariantList UserPreferences::savedProfiles() const
{
    return QJsonDocument::fromJson(m_settings.value("display/savedProfiles").toByteArray()).toVariant().toList();
}
QVariantList UserPreferences::profiles() const
{
    QVariantList result;
    const QList<QPair<QString, QString>> presets{{"classic", "Klasyczny ICM"}, {"full", "Wszystkie wykresy"}, {"basic", "Podstawowy"}, {"aviation", "Lotniczy"}, {"outdoor", "Na zewnątrz"}, {"custom", "Własny układ"}};
    for (const auto &preset : presets) result.append(QVariantMap{{"id", preset.first}, {"name", preset.second}, {"builtin", true}});
    for (const auto &entry : savedProfiles()) result.append(QVariantMap{{"id", entry.toMap()["id"]}, {"name", entry.toMap()["name"]}, {"builtin", false}});
    return result;
}
QVariantList UserPreferences::chartLayout() const
{
    const auto id = profile();
    if (id == "custom") {
        const auto saved = m_settings.value("display/customLayout").toByteArray();
        return saved.isEmpty() ? defaultLayout("classic") : normalizeLayout(QJsonDocument::fromJson(saved).toVariant().toList());
    }
    if (id.startsWith("user:")) for (const auto &entry : savedProfiles())
        if (entry.toMap()["id"] == id) return normalizeLayout(entry.toMap()["charts"].toList());
    return defaultLayout(id);
}
QVariantList UserPreferences::chartCatalog() const
{
    QVariantList result;
    for (const auto &chart : forecastCharts()) {
        QVariantList fields;
        for (const auto &key : chart.fields) for (const auto &field : forecastFields()) {
            if (field.key != key || field.kind == "lower" || key == "wchill_min") continue;
            fields.append(QVariantMap{{"key", key}, {"label", field.kind == "upper" ? QStringLiteral("Zakres min–max temperatury") : key == "wchill_max" ? QStringLiteral("Zakres min–max odczuwalnej") : field.label}});
        }
        result.append(QVariantMap{{"key", chart.key}, {"title", chart.title}, {"fields", fields}});
    }
    return result;
}
void UserPreferences::saveLayout(const QVariantList &layout)
{
    if (profile().startsWith("user:")) {
        auto saved = savedProfiles();
        for (auto &item : saved) { auto entry = item.toMap(); if (entry["id"] == profile()) { entry["charts"] = layout; item = entry; break; } }
        m_settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    } else {
        m_settings.setValue("display/customLayout", QJsonDocument::fromVariant(layout).toJson(QJsonDocument::Compact));
        m_settings.setValue("display/profile", "custom");
    }
    m_settings.sync(); emit displayChanged();
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
        auto fields = chart["fields"].toStringList();
        const auto affected = field == "airtmp_max" || field == "airtmp_min" ? QStringList{"airtmp_min", "airtmp_max"}
            : field == "wchill_max" || field == "wchill_min" ? QStringList{"wchill_min", "wchill_max"} : QStringList{field};
        for (const auto &item : affected) { fields.removeAll(item); if (enabled) fields.append(item); }
        chart["fields"] = fields; entry = chart; break;
    }
    saveLayout(normalizeLayout(layout));
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
    saved.append(QVariantMap{{"id", id}, {"name", name.trimmed().left(80)}, {"charts", chartLayout()}});
    m_settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_settings.setValue("display/profile", id); m_settings.sync(); emit displayChanged(); return id;
}
void UserPreferences::renameProfile(const QString &name)
{
    if (!profile().startsWith("user:") || name.trimmed().isEmpty()) return;
    auto saved = savedProfiles();
    for (auto &item : saved) { auto entry = item.toMap(); if (entry["id"] == profile()) { entry["name"] = name.trimmed().left(80); item = entry; break; } }
    m_settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_settings.sync(); emit displayChanged();
}
void UserPreferences::deleteProfile()
{
    if (!profile().startsWith("user:")) return;
    auto saved = savedProfiles();
    saved.removeIf([this](const QVariant &item) { return item.toMap()["id"] == profile(); });
    m_settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_settings.setValue("display/profile", "classic"); m_settings.sync(); emit displayChanged();
}
bool UserPreferences::isFavorite(double latitude, double longitude) const
{
    for (const auto &value : favorites()) {
        const auto place = value.toMap();
        if (std::abs(place["latitude"].toDouble() - latitude) < 0.000001 && std::abs(place["longitude"].toDouble() - longitude) < 0.000001) return true;
    }
    return false;
}
void UserPreferences::rememberPlace(const QString &name, double latitude, double longitude)
{
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180) return;
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
