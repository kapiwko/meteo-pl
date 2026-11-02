#pragma once
#include <QVariantMap>
#include <QVariantList>
#include <functional>
#include "ProfileCatalog.hpp"

// Pure profile catalog/editor. Persistence and transaction ownership stay outside.
class ProfileEditor : public ProfileCatalog {
public:
    ProfileEditor(QVariantMap state,QVariantList defaults):ProfileCatalog(std::move(state),std::move(defaults)) {}
    static QStringList stateKeys() { return {"profile","savedProfiles","deletedProfiles","customLayout","customChartHeight","customProfileName"}; }
    QVariantMap state() const { return m_store.values; }
    QString error() const { return m_error; }
    QVariantMap importDefinitions(const QVariantList &definitions,bool activate);
    void setProfile(const QString &value);
    void setVisibleGroups(const QStringList &value, const QString &profileId = {});
    QString createProfile(const QString &name);
    QString duplicateProfile(const QString &profileId = {}, bool activate = false);
    void renameProfile(const QString &name, const QString &profileId = {});
    void deleteProfile(const QString &profileId = {});
    void resetProfile(const QString &profileId = {});
    void restoreDeletedProfiles();
    void updateProfile(const QString &id = {});
    void setChartEnabled(const QString &key, bool enabled, const QString &profileId = {});
    void setChartField(const QString &chart, const QString &field, bool enabled, const QString &profileId = {});
    QVariantMap moveChart(const QString &key, int direction, const QString &profileId = {});
    QString createChart(const QString &title, const QString &profileId = {});
    void addChart(const QString &key, const QString &profileId = {});
    void deleteChart(const QString &key, const QString &profileId = {});
    bool setChartOptions(const QString &key, const QVariantMap &options, const QString &profileId = {});
    void setSeriesStyle(const QString &chart, const QString &field, const QVariantMap &options, const QString &profileId = {});
    void removeSeries(const QString &chart, const QString &field, const QString &profileId = {});
    QVariantMap restoreProfileState(const QVariantMap &before, const QVariantMap &after);
    QVariantMap profileCommand(const QVariantMap &command, const QString &profileId = {});
    QVariantMap undoProfileCommand(const QVariantMap &token);
    void setProfileChartHeightForProfile(const QString &id, int height);
    void saveLayout(const QString &profile, const QVariantList &layout);
    void saveProfileOptions(const QString &profile, const QVariantMap &options);
    QString createProfileFrom(const QString &name, const QString &sourceProfile, bool activate = true);
private:
    bool requireProfile(const QString &id);
    bool change(const std::function<void()> &operation) { operation(); return true; }
    QString m_error;
};
