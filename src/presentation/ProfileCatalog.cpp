#include "ProfileCatalog.hpp"
#include "ChartConfiguration.hpp"
#include "core/Localization.hpp"
#include <QSet>
#include <algorithm>

namespace {
// JSON persistence represents both Qt list types as arrays. Undo compares domain
// values rather than treating QStringList -> QVariantList as a foreign edit.
QVariant canonicalValue(const QVariant &value)
{
    if (value.metaType().id()==QMetaType::QVariantMap) {
        auto map=value.toMap();
        for (auto it=map.begin();it!=map.end();++it) it.value()=canonicalValue(it.value());
        return map;
    }
    if (value.metaType().id()==QMetaType::QVariantList || value.metaType().id()==QMetaType::QStringList) {
        QVariantList list;
        if (value.metaType().id()==QMetaType::QStringList) for (const auto &entry : value.toStringList()) list.append(entry);
        else for (const auto &entry : value.toList()) list.append(canonicalValue(entry));
        return list;
    }
    return value;
}
}

QVariantMap ProfileCatalog::factoryProfile(const QString &id) const
{
    for (const auto &entry : m_factoryProfiles)
        if (entry.toMap()["id"] == id) return entry.toMap();
    return {};
}

QString ProfileCatalog::profile() const
{
    const auto selected = m_store.value("profile", "classic").toString();
    const auto available = profiles();
    for (const auto &entry : available)
        if (entry.toMap()["id"] == selected) return selected;
    return available.isEmpty() ? QStringLiteral("classic") : available.first().toMap()["id"].toString();
}

QStringList ProfileCatalog::visibleGroups() const
{
    QStringList result;
    for (const auto &entry : chartLayout())
        if (entry.toMap()["enabled"].toBool())
            result.append(entry.toMap()["key"].toString());
    return result;
}

int ProfileCatalog::chartHeightForProfile(const QString &id) const
{
    const auto available = profiles();
    const bool listed = std::any_of(available.cbegin(), available.cend(), [&](const QVariant &entry) { return entry.toMap()["id"] == id; });
    if (!listed && !available.isEmpty()) return chartHeightForProfile(available.first().toMap()["id"].toString());
    if (id == "custom")
        return m_store.value("customChartHeight", 0).toInt();
    const auto height = factoryProfile(id).value("chartHeight", 0);
    for (const auto &value : savedProfiles())
        if (value.toMap()["id"] == id)
            return value.toMap().value("chartHeight", height).toInt();
    return height.toInt();
}

QVariantList ProfileCatalog::savedProfiles() const
{
    return m_store.value("savedProfiles").toList();
}

QVariantList ProfileCatalog::profiles() const
{
    QVariantList result;
    const auto saved = savedProfiles();
    const auto deleted = m_store.value("deletedProfiles").toStringList();
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
    if (m_store.contains("customLayout"))
        result.append(QVariantMap{{"id", "custom"},
            {"name", m_store.value("customProfileName", Meteo::i18n("Custom layout"))}, {"builtin", false}});
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

bool ProfileCatalog::hasDeletedProfiles() const { return !m_store.value("deletedProfiles").toStringList().isEmpty(); }

QVariantList ProfileCatalog::chartLayout() const { return chartLayoutForProfile(profile()); }

QVariantList ProfileCatalog::chartLayoutForProfile(const QString &id) const
{
    const auto available = profiles();
    const bool listed = std::any_of(available.cbegin(), available.cend(), [&](const QVariant &entry) { return entry.toMap()["id"] == id; });
    if (!listed && !available.isEmpty()) return chartLayoutForProfile(available.first().toMap()["id"].toString());
    if (id == "custom") {
        const auto saved = m_store.value("customLayout").toList();
        return saved.isEmpty() ? m_factoryProfiles.first().toMap()["charts"].toList()
                               : normalizeChartLayout(saved);
    }
    for (const auto &value : savedProfiles()) {
        const auto entry = value.toMap();
        if (entry["id"] == id && entry.contains("charts")) return normalizeChartLayout(entry["charts"].toList());
    }
    return factoryProfile(id).value("charts", m_factoryProfiles.first().toMap()["charts"]).toList();
}

QVariantMap ProfileCatalog::profileState(const QString &id) const
{
    QVariantMap state{{"id", id}, {"deleted", m_store.value("deletedProfiles").toStringList().contains(id)}, {"override", QVariantMap{}}};
    for (const auto &entry : savedProfiles())
        if (entry.toMap()["id"] == id) { state["override"] = entry; break; }
    if (id == "custom") {
        state["hasLayout"] = m_store.contains("customLayout");
        state["layout"] = m_store.value("customLayout");
        state["height"] = m_store.value("customChartHeight", 0);
        state["name"] = m_store.value("customProfileName", "");
    }
    return canonicalValue(state).toMap();
}

QVariantMap ProfileCatalog::profileSnapshot() const
{
    QSet<QString> ids{"custom"};
    for (const auto &entry : m_factoryProfiles) ids.insert(entry.toMap()["id"].toString());
    for (const auto &entry : savedProfiles()) ids.insert(entry.toMap()["id"].toString());
    QVariantMap result;
    for (const auto &id : ids) result[id] = profileState(id);
    return result;
}
