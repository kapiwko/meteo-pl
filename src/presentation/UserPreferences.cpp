#include "UserPreferences.hpp"
#include "ForecastFields.hpp"
#include "ChartConfiguration.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <QUuid>
#include <algorithm>

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
bool UserPreferences::showLegend() const { return m_settings.value("display/showLegend", true).toBool(); }
void UserPreferences::setShowLegend(bool value) { if (value==showLegend()) return; m_settings.setValue("display/showLegend",value);m_settings.sync();emit displayChanged(); }
int UserPreferences::chartHeight() const { return std::clamp(m_settings.value("display/chartHeight", 174).toInt(),64,800); }
int UserPreferences::profileChartHeight() const
{
    if (profile() == "custom") return m_settings.value("display/customChartHeight", 0).toInt();
    for (const auto &value : savedProfiles()) if (value.toMap()["id"] == profile()) return value.toMap().value("chartHeight",0).toInt();
    return 0;
}
void UserPreferences::setChartHeight(int value)
{
    value=std::clamp(value,64,800);
    if (value==chartHeight()) return;
    m_settings.setValue("display/chartHeight",value);m_settings.sync();emit displayChanged();
}
void UserPreferences::setProfileChartHeight(int value)
{
    value=value ? std::clamp(value,64,800) : 0;
    if (value==profileChartHeight()) return;
    if (profile().startsWith("user:")) {
        auto saved=savedProfiles();
        for (auto &item:saved) { auto entry=item.toMap();if (entry["id"]==profile()) { entry["chartHeight"]=value;item=entry;break; } }
        m_settings.setValue("display/savedProfiles",QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    } else {
        if (profile()!="custom") m_settings.setValue("display/customLayout",QJsonDocument::fromVariant(chartLayout()).toJson(QJsonDocument::Compact));
        m_settings.setValue("display/profile","custom");m_settings.setValue("display/customChartHeight",value);
    }
    m_settings.sync();emit displayChanged();
}
QString UserPreferences::renderer() const { return m_settings.value("display/renderer", "gpu").toString() == "cpu" ? "cpu" : "gpu"; }
void UserPreferences::setRenderer(const QString &value)
{
    if ((value != "gpu" && value != "cpu") || value == renderer()) return;
    m_settings.setValue("display/renderer", value); m_settings.sync(); emit displayChanged();
}
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
QString UserPreferences::settingsFile() const { return m_settings.fileName(); }

QVariantMap UserPreferences::exportProfiles(const QUrl &url, bool allSaved) const
{
    if (!url.isLocalFile()) return {{"ok", false}, {"error", "Wybierz plik lokalny."}};
    QVariantList exported;
    if (allSaved) {
        for (const auto &value : savedProfiles()) {
            const auto entry = value.toMap();
            exported.append(QVariantMap{{"name", entry["name"]}, {"charts", normalizeChartLayout(entry["charts"].toList())}, {"chartHeight",entry.value("chartHeight",0)}});
        }
        if (m_settings.contains("display/customLayout")) exported.append(QVariantMap{{"name", "Własny układ"},
            {"charts", normalizeChartLayout(QJsonDocument::fromJson(m_settings.value("display/customLayout").toByteArray()).toVariant().toList())}, {"chartHeight",m_settings.value("display/customChartHeight",0)}});
    }
    if (exported.isEmpty()) {
        QString name;
        for (const auto &value : profiles()) if (value.toMap()["id"] == profile()) name = value.toMap()["name"].toString();
        exported.append(QVariantMap{{"name", name}, {"charts", chartLayout()}, {"chartHeight",profileChartHeight()}});
    }
    const auto data = QJsonDocument(QJsonObject{{"format", "pl.meteo.chart-profiles"}, {"version", 1},
        {"profiles", QJsonArray::fromVariantList(exported)}}).toJson(QJsonDocument::Indented);
    if (exported.size() > 100 || data.size() > 16 * 1024 * 1024)
        return {{"ok", false}, {"error", "Za dużo profili w jednym pliku. Wyeksportuj je osobno."}};
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return {{"ok", false}, {"error", "Nie udało się zapisać pliku: " + file.errorString()}};
    return {{"ok", true}, {"count", exported.size()}, {"path", url.toLocalFile()}};
}

QVariantMap UserPreferences::importProfiles(const QUrl &url)
{
    if (!url.isLocalFile()) return {{"ok", false}, {"error", "Wybierz plik lokalny."}};
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) return {{"ok", false}, {"error", "Nie udało się otworzyć pliku: " + file.errorString()}};
    if (file.size() > 16 * 1024 * 1024) return {{"ok", false}, {"error", "Plik profili jest zbyt duży (maksymalnie 16 MiB)."}};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {{"ok", false}, {"error", "Plik nie zawiera poprawnego dokumentu JSON."}};
    const auto root = document.object();
    if (root["format"] != "pl.meteo.chart-profiles" || root["version"].toDouble(-1) != 1)
        return {{"ok", false}, {"error", "Nieobsługiwany format lub wersja pliku profili."}};
    const auto imported = root["profiles"].toArray();
    if (imported.isEmpty() || imported.size() > 100)
        return {{"ok", false}, {"error", "Plik musi zawierać od 1 do 100 profili."}};
    auto saved = savedProfiles();
    QSet<QString> names;
    for (const auto &value : profiles()) names.insert(value.toMap()["name"].toString());
    QString selected;
    for (const auto &value : imported) {
        const auto entry = value.toObject();
        const auto baseName = entry["name"].toString().trimmed().left(80);
        const auto source = entry["charts"].toArray();
        const auto charts = normalizeChartLayout(source.toVariantList());
        bool valid = !baseName.isEmpty() && entry["charts"].isArray() && source.size() <= 50 && charts.size() == source.size();
        for (qsizetype i = 0; valid && i < source.size(); ++i) {
            const auto series = source[i].toObject()["series"];
            valid = series.isArray() && charts[i].toMap()["series"].toList().size() == series.toArray().size();
        }
        if (!valid) return {{"ok", false}, {"error", "Nieprawidłowa definicja profilu lub nieznana seria. Nie zaimportowano żadnego profilu."}};
        QString name = baseName;
        for (int suffix = 2; names.contains(name); ++suffix) name = baseName.left(70) + " (" + QString::number(suffix) + ")";
        names.insert(name);
        const auto id = "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto height=entry["chartHeight"].toInt();
        saved.append(QVariantMap{{"id", id}, {"name", name}, {"charts", charts}, {"chartHeight",height ? std::clamp(height,64,800) : 0}});
        if (selected.isEmpty()) selected = id;
    }
    m_settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    m_settings.setValue("display/profile", selected);
    m_settings.sync();
    emit displayChanged();
    return {{"ok", true}, {"count", imported.size()}};
}
void UserPreferences::saveLayout(const QVariantList &layout)
{
    const auto normalized = normalizeChartLayout(layout);
    if (normalized == chartLayout()) return;
    if (profile().startsWith("user:")) {
        auto saved = savedProfiles();
        for (auto &item : saved) { auto entry = item.toMap(); if (entry["id"] == profile()) { entry["charts"] = normalized; item = entry; break; } }
        m_settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    } else {
        if (profile()!="custom") m_settings.setValue("display/customChartHeight",profileChartHeight());
        m_settings.setValue("display/customLayout", QJsonDocument::fromVariant(normalized).toJson(QJsonDocument::Compact));
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
    layout.append(QVariantMap{{"key", key}, {"title", title.trimmed().isEmpty() ? "Nowy wykres" : title.trimmed().left(80)},
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
