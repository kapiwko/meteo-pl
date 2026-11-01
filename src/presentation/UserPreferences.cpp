#include "UserPreferences.hpp"
#include "ProfileEditor.hpp"
#include "SavedPlaces.hpp"
#include "core/DocumentFile.hpp"
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
QVariantList UserPreferences::favorites() const
{ const SavedPlaces places(readPlaces("places/favorites"),readPlaces("places/recent")); return places.favorites(); }
QVariantList UserPreferences::recentPlaces() const
{ const SavedPlaces places(readPlaces("places/favorites"),readPlaces("places/recent")); return places.recentPlaces(); }
QString UserPreferences::profile() const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.profile(); }
QStringList UserPreferences::visibleGroups() const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.visibleGroups(); }
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
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.chartHeightForProfile(id); }
void UserPreferences::setChartHeight(int value)
{
    change([&] {
        value = std::clamp(value, 64, 800);
        if (value == chartHeight())
            return;
        m_store.setValue("display/chartHeight", value);
    });
}
void UserPreferences::setProfileChartHeight(int value) { setProfileChartHeightForProfile(profile(), value); }
void UserPreferences::setProfileChartHeightForProfile(const QString &selectedProfile, int value)
{ editProfiles([&](ProfileEditor &editor) { editor.setProfileChartHeightForProfile(selectedProfile,value); }); }
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
QString UserPreferences::theme() const
{
    const auto value = m_store.value("display/theme", "automatic").toString();
    return QStringList{"automatic", "light", "dark"}.contains(value) ? value : QStringLiteral("automatic");
}
void UserPreferences::setTheme(const QString &value)
{
    change([&] {
        if (value == theme() || !QStringList{"automatic", "light", "dark"}.contains(value)) return;
        m_store.setValue("display/theme", value);
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
{ editProfiles([&](ProfileEditor &editor) { editor.setProfile(value); }); }
void UserPreferences::setVisibleGroups(const QStringList &value)
{ const auto targetProfile=profile(); editProfiles([&](ProfileEditor &editor) { editor.setVisibleGroups(value,targetProfile); }); }
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

QVariantList UserPreferences::profiles() const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.profiles(); }
bool UserPreferences::hasDeletedProfiles() const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.hasDeletedProfiles(); }
QVariantList UserPreferences::chartLayout() const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.chartLayout(); }
QVariantList UserPreferences::chartLayoutForProfile(const QString &id) const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.chartLayoutForProfile(id); }
QVariantList UserPreferences::chartCatalog() const { return defaultChartCatalog(); }
QVariantMap UserPreferences::chartAppearance() const { return defaultChartAppearance(); }
QVariantList UserPreferences::seriesCatalog() const { return chartSeriesCatalog(); }
QString UserPreferences::settingsFile() const { return m_store.fileName(); }

QVariantMap UserPreferences::exportProfiles(const QUrl &url, bool allSaved) const
{
    QStringList ids;
    if (allSaved) for (const auto &entry : profiles()) ids.append(entry.toMap()["id"].toString());
    else ids.append(profile());
    return exportProfilesByIds(url, ids);
}
QVariantMap UserPreferences::exportProfilesByIds(const QUrl &url, const QStringList &ids) const
{
    QVariantList exported;
    for (const auto &entry : profiles()) {
        const auto item = entry.toMap();
        const auto id = item["id"].toString();
        if (!ids.contains(id)) continue;
        exported.append(QVariantMap{{"name", item["name"]}, {"charts", chartLayoutForProfile(id)},
            {"chartHeight", chartHeightForProfile(id)}, {"revision", std::max(1, item.value("revision", 1).toInt())}});
    }
    if (exported.isEmpty()) return {{"ok", false}, {"error", Meteo::i18n("Select at least one profile.")}};
    QString error;
    if (!ProfileFileCodec::write(url, exported, &error)) return {{"ok", false}, {"error", error}};
    return {{"ok", true}, {"count", exported.size()}, {"path", Meteo::documentDisplayPath(url)}};
}

QVariantMap UserPreferences::importProfiles(const QUrl &url, bool activate)
{
    QString error;
    const auto imported = ProfileFileCodec::read(url, &error);
    if (!imported)
        return {{"ok", false}, {"error", error}};
    QVariantMap result;
    const bool persisted=editProfiles([&](ProfileEditor &editor) { result=editor.importDefinitions(*imported,activate); });
    return persisted ? result : QVariantMap{{"ok",false},{"error",m_error}};
}

QVariantMap UserPreferences::profileState(const QString &id) const
{ const ProfileEditor editor(profileValues(),m_factoryProfiles); return editor.profileState(id); }
QVariantMap UserPreferences::profileCommand(const QVariantMap &command, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; QVariantMap result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.profileCommand(command,targetProfile); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }
QVariantMap UserPreferences::undoProfileCommand(const QVariantMap &token)
{ QVariantMap result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.undoProfileCommand(token); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }
QVariantMap UserPreferences::restoreProfileState(const QVariantMap &before, const QVariantMap &after)
{ QVariantMap result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.restoreProfileState(before,after); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }

void UserPreferences::setChartEnabled(const QString &key, bool enabled, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.setChartEnabled(key,enabled,targetProfile); }); }
void UserPreferences::setChartField(const QString &key, const QString &field, bool enabled, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.setChartField(key,field,enabled,targetProfile); }); }
QString UserPreferences::createChart(const QString &title, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; QString result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.createChart(title,targetProfile); }); return saved ? result : decltype(result){}; }
void UserPreferences::deleteChart(const QString &key, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.deleteChart(key,targetProfile); }); }
void UserPreferences::addChart(const QString &key, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.addChart(key,targetProfile); }); }
bool UserPreferences::setChartOptions(const QString &key, const QVariantMap &options, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; bool result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.setChartOptions(key,options,targetProfile); }); return saved ? result : decltype(result){}; }
void UserPreferences::setSeriesStyle(const QString &key, const QString &field, const QVariantMap &options, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.setSeriesStyle(key,field,options,targetProfile); }); }
void UserPreferences::removeSeries(const QString &key, const QString &field, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.removeSeries(key,field,targetProfile); }); }
QVariantMap UserPreferences::moveChart(const QString &key, int direction, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; QVariantMap result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.moveChart(key,direction,targetProfile); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }
QString UserPreferences::duplicateProfile(const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; QString result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.duplicateProfile(targetProfile,profileId.isEmpty()); }); return saved ? result : decltype(result){}; }
void UserPreferences::renameProfile(const QString &name, const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.renameProfile(name,targetProfile); }); }
void UserPreferences::deleteProfile(const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.deleteProfile(targetProfile); }); }
void UserPreferences::resetProfile(const QString &profileId)
{ const auto targetProfile=profileId.isEmpty()?profile():profileId; editProfiles([&](ProfileEditor &editor) { editor.resetProfile(targetProfile); }); }
void UserPreferences::restoreDeletedProfiles()
{ editProfiles([&](ProfileEditor &editor) { editor.restoreDeletedProfiles(); }); }
void UserPreferences::updateProfile(const QString &id)
{ const auto targetProfile=id.isEmpty()?profile():id; editProfiles([&](ProfileEditor &editor) { editor.updateProfile(targetProfile); }); }
bool UserPreferences::isFavorite(double latitude, double longitude) const
{ const SavedPlaces places(readPlaces("places/favorites"),readPlaces("places/recent")); return places.isFavorite(latitude,longitude); }
QVariantMap UserPreferences::placeAt(double latitude, double longitude) const
{ const SavedPlaces places(readPlaces("places/favorites"),readPlaces("places/recent")); return places.placeAt(latitude,longitude); }
void UserPreferences::savePlace(const QString &name, double latitude, double longitude)
{ editPlaces([&](SavedPlaces &places) { places.savePlace(name,latitude,longitude); }); }
QVariantMap UserPreferences::editPlace(const QString &id, const QString &name, double latitude, double longitude)
{ QVariantMap result; const bool saved=editPlaces([&](SavedPlaces &places) { result=places.editPlace(id,name,latitude,longitude); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }
QVariantMap UserPreferences::deletePlace(const QString &id)
{ QVariantMap result; const bool saved=editPlaces([&](SavedPlaces &places) { result=places.deletePlace(id); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }
QVariantMap UserPreferences::restorePlace(const QVariantMap &place, int index)
{ QVariantMap result; const bool saved=editPlaces([&](SavedPlaces &places) { result=places.restorePlace(place,index); }); return saved ? result : QVariantMap{{"ok",false},{"error",m_error}}; }
void UserPreferences::removePlace(double latitude, double longitude)
{ editPlaces([&](SavedPlaces &places) { places.removePlace(latitude,longitude); }); }
void UserPreferences::rememberPlace(const QString &name, double latitude, double longitude)
{ editPlaces([&](SavedPlaces &places) { places.rememberPlace(name,latitude,longitude); }); }
void UserPreferences::toggleFavorite(const QString &name, double latitude, double longitude)
{ editPlaces([&](SavedPlaces &places) { places.toggleFavorite(name,latitude,longitude); }); }

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

QString UserPreferences::createProfile(const QString &name)
{ const auto source=profile(); QString result{}; const bool saved=editProfiles([&](ProfileEditor &editor) { result=editor.createProfileFrom(name,source); }); return saved ? result : decltype(result){}; }

QStringList UserPreferences::seriesRenderers(const QString &key) const { return ::seriesRenderers(key); }

QStringList UserPreferences::seriesFills(const QString &key) const { return ::seriesFills(key); }

QVariantMap UserPreferences::profileValues() const
{
    QVariantMap result;
    for (const auto &key : ProfileEditor::stateKeys()) {
        const auto stored="display/"+key;
        if (!m_store.contains(stored)) continue;
        const auto value=m_store.value(stored);
        result[key]=(key=="savedProfiles" || key=="customLayout") ? QVariant(QJsonDocument::fromJson(value.toByteArray()).toVariant().toList()) : value;
    }
    return result;
}
bool UserPreferences::editProfiles(const std::function<void(ProfileEditor &)> &operation)
{
    QString validationError;
    const bool saved=change([&] {
        const auto before=profileValues(); ProfileEditor editor(before,m_factoryProfiles);
        operation(editor); validationError=editor.error(); const auto after=editor.state();
        for (const auto &key : ProfileEditor::stateKeys()) {
            if (before.contains(key)==after.contains(key) && before.value(key)==after.value(key)) continue;
            const auto stored="display/"+key;
            if (!after.contains(key)) m_store.remove(stored);
            else m_store.setValue(stored,(key=="savedProfiles" || key=="customLayout") ? QVariant(QJsonDocument::fromVariant(after[key]).toJson(QJsonDocument::Compact)) : after[key]);
        }
    });
    if (saved && !validationError.isEmpty() && m_error!=validationError) { m_error=validationError; emit errorChanged(); }
    return saved;
}

bool UserPreferences::editPlaces(const std::function<void(SavedPlaces &)> &operation)
{
    return change([&] {
        const auto before=readPlaces("places/favorites"),recent=readPlaces("places/recent");
        SavedPlaces places(before,recent); operation(places);
        if (places.favorites()!=before) writePlaces("places/favorites",places.favorites());
        if (places.recentPlaces()!=recent) writePlaces("places/recent",places.recentPlaces());
    });
}
