#include "ProfileEditor.hpp"
#include "ChartConfiguration.hpp"
#include "core/Localization.hpp"
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

bool ProfileEditor::requireProfile(const QString &id)
{
    if (findProfileLayout(id)) return true;
    m_error=Meteo::i18n("This profile no longer exists.");
    return false;
}

void ProfileEditor::setProfileChartHeightForProfile(const QString &selectedProfile, int value)
{
    if (!requireProfile(selectedProfile)) return;
    value = value ? std::clamp(value, 64, 800) : 0;
    if (value == chartHeightForProfile(selectedProfile))
        return;
    if (selectedProfile == "custom") {
        m_store.setValue("customChartHeight", value);
    } else saveProfileOptions(selectedProfile, {{"chartHeight", value}});
}

void ProfileEditor::setProfile(const QString &value)
{
    if (value == profile())
        return;
    bool valid = false;
    for (const auto &entry : profiles())
        if (entry.toMap()["id"] == value)
            valid = true;
    if (!valid)
        return;
    m_store.setValue("profile", value);
}

void ProfileEditor::setVisibleGroups(const QStringList &value, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
    auto layout = chartLayoutForProfile(selectedProfile);
    for (auto &entry : layout) {
        auto chart = entry.toMap();
        chart["enabled"] = value.contains(chart["key"].toString());
        entry = chart;
    }
    saveLayout(selectedProfile, layout);
}

QVariantMap ProfileEditor::profileCommand(const QVariantMap &command, const QString &profileId)
{
    const auto id = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(id)) return {{"ok",false},{"code","NotFound"},{"error",m_error}};
    QVariantMap result{{"ok",false}};
    const auto before = profileSnapshot();
    const auto type = command.value("type").toString();
    if (type == "deleteChart") deleteChart(command.value("chart").toString(),id);
    else if (type == "removeSeries") removeSeries(command.value("chart").toString(),command.value("field").toString(),id);
    else if (type == "deleteProfile") deleteProfile(id);
    else if (type == "resetProfile") resetProfile(id);
    else { result["error"] = Meteo::i18n("Unknown profile command."); return result; }
    const auto after = profileSnapshot();
    QSet<QString> ids;
    for (auto it=before.cbegin();it!=before.cend();++it) ids.insert(it.key());
    for (auto it=after.cbegin();it!=after.cend();++it) ids.insert(it.key());
    QVariantList records;
    for (const auto &key : ids) {
        const QVariantMap empty{{"id",key},{"deleted",false},{"override",QVariantMap{}}};
        const auto previous=before.value(key,empty), next=after.value(key,empty);
        if (previous!=next) records.append(QVariantMap{{"before",previous},{"after",next}});
    }
    result={{"ok",true},{"id",id},{"undo",QVariantMap{{"id",id},{"records",records}}}};
    return result;
}

QVariantMap ProfileEditor::undoProfileCommand(const QVariantMap &token)
{
    QVariantMap result{{"ok",false},{"error",Meteo::i18n("This profile has changed since that action. It cannot be undone.")}};
    const auto records=token.value("records").toList();
    if (records.isEmpty()) return result;
    for (const auto &entry : records) {
        const auto record=entry.toMap(), after=record.value("after").toMap(), before=record.value("before").toMap();
        const auto id=after.value("id").toString();
        if (id.isEmpty() || before.value("id")!=id || profileState(id)!=after) return result;
    }
    for (const auto &entry : records) {
        const auto record=entry.toMap();
        restoreProfileState(record["before"].toMap(),record["after"].toMap());
    }
    result={{"ok",true},{"id",token["id"]}};
    return result;
}

QVariantMap ProfileEditor::restoreProfileState(const QVariantMap &before, const QVariantMap &after)
{
    QVariantMap result{{"ok", false}};
    const auto id = before.value("id").toString();
    if (id.isEmpty() || after.value("id").toString() != id || profileState(id) != after) {
        result["error"] = Meteo::i18n("This profile has changed since that action. It cannot be undone.");
        return result;
    }
    auto saved = savedProfiles();
    saved.removeIf([&](const QVariant &entry) { return entry.toMap()["id"].toString() == id; });
    const auto original = before.value("override").toMap();
    if (!original.isEmpty()) saved.append(original);
    m_store.setValue("savedProfiles", saved);
    auto deleted = m_store.value("deletedProfiles").toStringList();
    deleted.removeAll(id);
    if (before.value("deleted").toBool()) deleted.append(id);
    m_store.setValue("deletedProfiles", deleted);
    if (id == "custom") {
        if (before.value("hasLayout").toBool()) m_store.setValue("customLayout", before["layout"]);
        else m_store.remove("customLayout");
        m_store.setValue("customChartHeight", before.value("height", 0));
        m_store.setValue("customProfileName", before.value("name", ""));
    }
    result = {{"ok", true}, {"id", id}};
    return result;
}

void ProfileEditor::saveLayout(const QString &id, const QVariantList &layout)
{
    if (!requireProfile(id)) return;
    const auto normalized = normalizeChartLayout(layout);
    if (normalized == chartLayoutForProfile(id))
        return;
    if (id == "custom") {
        m_store.setValue("customLayout", normalized);
    } else saveProfileOptions(id, {{"charts", normalized}});
}

void ProfileEditor::saveProfileOptions(const QString &id, const QVariantMap &options)
{
    if (!requireProfile(id)) return;
    auto saved = savedProfiles();
    for (auto &value : saved) {
        auto entry = value.toMap();
        if (entry["id"] != id) continue;
        for (auto it = options.cbegin(); it != options.cend(); ++it) entry[it.key()] = it.value();
        value = entry;
        m_store.setValue("savedProfiles", saved);
        return;
    }
    auto entry = factoryProfile(id);
    if (entry.isEmpty() || m_store.value("deletedProfiles").toStringList().contains(id)) return;
    entry["baseRevision"] = entry.take("revision");
    for (auto it = options.cbegin(); it != options.cend(); ++it) entry[it.key()] = it.value();
    saved.append(entry);
    m_store.setValue("savedProfiles", saved);
}

void ProfileEditor::setChartEnabled(const QString &key, bool enabled, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
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
}

void ProfileEditor::setChartField(const QString &key, const QString &field, bool enabled, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
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
}

QString ProfileEditor::createChart(const QString &title, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return {};
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
}

void ProfileEditor::deleteChart(const QString &key, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
    auto layout = chartLayoutForProfile(selectedProfile);
    layout.removeIf([&key](const QVariant &value) { return value.toMap()["key"] == key; });
    saveLayout(selectedProfile, layout);
}

void ProfileEditor::addChart(const QString &key, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
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
}

bool ProfileEditor::setChartOptions(const QString &key, const QVariantMap &options, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return false;
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
        for (const auto &axis : {QStringLiteral("left"), QStringLiteral("right")}) {
            const auto low = chart.value(axis + "Min"), high = chart.value(axis + "Max");
            for (const auto &bound : {low, high}) {
                if (!bound.isValid() || bound.isNull()) continue;
                bool ok = false; const auto number = bound.toDouble(&ok);
                if (!ok || !std::isfinite(number)) { m_error = Meteo::i18n("Enter a valid axis limit."); return false; }
            }
            if (low.isValid() && !low.isNull() && high.isValid() && !high.isNull() && low.toDouble() >= high.toDouble()) {
                m_error = Meteo::i18n("The minimum must be lower than the maximum."); return false;
            }
        }
        value = chart;
        break;
    }
    saveLayout(selectedProfile, normalizeChartLayout(layout));
    m_error.clear();
    return true;
}

void ProfileEditor::setSeriesStyle(const QString &key, const QString &field, const QVariantMap &options, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
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
            if ((options.contains("color") || options.contains("darkColor"))
                && QStringList{"storm","flash"}.contains(style["render"].toString())) style["eventColors"]=false;
            entry = normalizeSeriesStyle(style);
            break;
        }
        chart["series"] = series;
        value = chart;
        break;
    }
    saveLayout(selectedProfile, normalizeChartLayout(layout));
}

void ProfileEditor::removeSeries(const QString &key, const QString &field, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
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
}

QVariantMap ProfileEditor::moveChart(const QString &key, int direction, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return {{"ok",false},{"code","NotFound"},{"error",m_error}};
    QVariantMap result{{"ok", false}, {"error", Meteo::i18n("This chart no longer exists.")}};
    auto layout = chartLayoutForProfile(selectedProfile);
    for (qsizetype index = 0; index < layout.size(); ++index) {
        if (layout[index].toMap()["key"] != key) continue;
        const auto next = index + direction;
        if (next < 0 || next >= layout.size()) { result["error"] = Meteo::i18n("The chart cannot be moved further."); return result; }
        layout.swapItemsAt(index, next);
        saveLayout(selectedProfile, layout);
        result = {{"ok", true}};
        return result;
    }
    return result;
}

QString ProfileEditor::createProfileFrom(const QString &name, const QString &sourceProfile, bool activate)
{
    if (!requireProfile(sourceProfile)) return {};
    if (name.trimmed().isEmpty())
        return {};
    const auto id = "user:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto saved = savedProfiles();
    saved.append(QVariantMap{{"id", id},
                             {"name", name.trimmed().left(80)},
                             {"charts", chartLayoutForProfile(sourceProfile)},
                             {"chartHeight", chartHeightForProfile(sourceProfile)}});
    m_store.setValue("savedProfiles", saved);
    if (activate) m_store.setValue("profile", id);
    return id;
}

QString ProfileEditor::duplicateProfile(const QString &profileId, bool activate)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return {};
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
    return createProfileFrom(name, selectedProfile, profileId.isEmpty() || activate);
}

void ProfileEditor::renameProfile(const QString &name, const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
    if (name.trimmed().isEmpty()) return;
    if (selectedProfile == "custom") m_store.setValue("customProfileName", name.trimmed().left(80));
    else saveProfileOptions(selectedProfile, {{"name", name.trimmed().left(80)}});
}

void ProfileEditor::deleteProfile(const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
    if (profiles().size() <= 1) return;
    const bool wasSelected = profile() == selectedProfile;
    if (selectedProfile == "custom") {
        m_store.remove("customLayout");
        m_store.remove("customChartHeight");
        m_store.remove("customProfileName");
    } else if (!factoryProfile(selectedProfile).isEmpty()) {
        auto deleted = m_store.value("deletedProfiles").toStringList();
        if (!deleted.contains(selectedProfile)) deleted.append(selectedProfile);
        m_store.setValue("deletedProfiles", deleted);
    }
    auto saved = savedProfiles();
    saved.removeIf([&](const QVariant &item) { return item.toMap()["id"] == selectedProfile; });
    m_store.setValue("savedProfiles", saved);
    if (wasSelected) m_store.setValue("profile", profiles().first().toMap()["id"]);
}

void ProfileEditor::resetProfile(const QString &profileId)
{
    const auto selectedProfile = profileId.isEmpty() ? profile() : profileId;
    if (!requireProfile(selectedProfile)) return;
    if (factoryProfile(selectedProfile).isEmpty()) return;
    auto saved = savedProfiles();
    for (const auto &entry : saved)
        if (entry.toMap()["id"] == selectedProfile
            && entry.toMap().value("baseRevision", 0).toInt() < factoryProfile(selectedProfile)["revision"].toInt()) {
            updateProfile(selectedProfile);
            return;
        }
    saved.removeIf([&](const QVariant &item) { return item.toMap()["id"] == selectedProfile; });
    m_store.setValue("savedProfiles", saved);
}

void ProfileEditor::restoreDeletedProfiles()
{
    m_store.remove("deletedProfiles");
}

void ProfileEditor::updateProfile(const QString &id)
{
    const auto selectedProfile = id.isEmpty() ? profile() : id;
    if (!requireProfile(selectedProfile)) return;
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
        m_store.setValue("savedProfiles", saved);
        return;
    }
}

QString ProfileEditor::createProfile(const QString &name) { return createProfileFrom(name, profile()); }

QVariantMap ProfileEditor::importDefinitions(const QVariantList &definitions,bool activate)
{
    auto saved=savedProfiles(); QSet<QString> names;
    for (const auto &value : profiles()) names.insert(value.toMap()["name"].toString());
    QStringList ids;
    for (const auto &value : definitions) {
        auto entry=value.toMap(); const auto base=entry["name"].toString(); auto name=base;
        for (int number=2;names.contains(name);++number) name=base.left(70)+QStringLiteral(" (%1)").arg(number);
        names.insert(name); const auto id="user:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry["id"]=id; entry["name"]=name; saved.append(entry); ids.append(id);
    }
    m_store.setValue("savedProfiles",saved);
    if (activate && !ids.isEmpty()) m_store.setValue("profile",ids.first());
    return {{"ok",true},{"count",ids.size()},{"ids",ids}};
}
