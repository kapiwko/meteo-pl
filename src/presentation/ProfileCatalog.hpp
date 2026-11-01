#pragma once
#include <QVariantMap>
#include <QVariantList>

class ProfileValues {
public:
    QVariantMap values;
    QVariant value(const QString &key,const QVariant &fallback={}) const { return values.value(key,fallback); }
    bool contains(const QString &key) const { return values.contains(key); }
    void setValue(const QString &key,const QVariant &value) { values[key]=value; }
    void remove(const QString &key) { values.remove(key); }
};
class ProfileCatalog {
public:
    ProfileCatalog(QVariantMap state,QVariantList defaults):m_store{std::move(state)},m_factoryProfiles(std::move(defaults)) {}
    QString profile() const;
    QStringList visibleGroups() const;
    QVariantList profiles() const;
    bool hasDeletedProfiles() const;
    QVariantList chartLayout() const;
    QVariantList chartLayoutForProfile(const QString &id) const;
    int chartHeightForProfile(const QString &id) const;
    QVariantMap profileState(const QString &id) const;
    QVariantMap profileSnapshot() const;
    QVariantMap factoryProfile(const QString &id) const;
    QVariantList savedProfiles() const;
protected:
    ProfileValues m_store;
    QVariantList m_factoryProfiles;
};
