#include "PreferencesStore.hpp"
#include "ChartConfiguration.hpp"
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <cmath>

PreferencesStore::PreferencesStore(QObject *parent)
    : QObject(parent),
      m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("meteo-pl"), QStringLiteral("desktop"))
{
    migrate();
    m_places = placesSnapshot();
    m_display = displaySnapshot();
    m_watcher.addPath(QFileInfo(m_settings.fileName()).absolutePath());
    if (QFile::exists(m_settings.fileName()))
        m_watcher.addPath(m_settings.fileName());
    m_reload.setSingleShot(true);
    m_reload.setInterval(40);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_reload.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reload.start(); });
    connect(&m_reload, &QTimer::timeout, this, [this] {
        // QSettings replaces the file atomically; the directory watch survives that rename.
        if (!m_watcher.files().contains(m_settings.fileName()) && QFile::exists(m_settings.fileName()))
            m_watcher.addPath(m_settings.fileName());
        sync();
    });
}
QVariant PreferencesStore::value(const QString &key, const QVariant &fallback) const
{
    return m_settings.value(key, fallback);
}
bool PreferencesStore::contains(const QString &key) const { return m_settings.contains(key); }
void PreferencesStore::setValue(const QString &key, const QVariant &value) { m_settings.setValue(key, value); }
void PreferencesStore::remove(const QString &key) { m_settings.remove(key); }
QString PreferencesStore::fileName() const { return m_settings.fileName(); }
void PreferencesStore::sync()
{
    m_settings.sync();
    const auto places = placesSnapshot();
    const auto display = displaySnapshot();
    const bool placesUpdated = places != m_places;
    const bool displayUpdated = display != m_display;
    m_places = places;
    m_display = display;
    if (placesUpdated)
        emit placesChanged();
    if (displayUpdated)
        emit displayChanged();
}
QByteArray PreferencesStore::placesSnapshot() const
{
    return m_settings.value("places/favorites").toByteArray() + '\n' + m_settings.value("places/recent").toByteArray();
}
QByteArray PreferencesStore::displaySnapshot() const
{
    QVariantMap values;
    for (const auto &key : m_settings.allKeys())
        if (key.startsWith("display/"))
            values.insert(key, m_settings.value(key));
    QByteArray snapshot;
    QDataStream stream(&snapshot, QIODevice::WriteOnly);
    stream << values;
    return snapshot;
}
void PreferencesStore::migrate()
{
    if (m_settings.value("display/layoutVersion", 0).toInt() < 2) {
        m_settings.setValue("display/profile", "classic");
        m_settings.setValue("display/layoutVersion", 2);
    }
    if (m_settings.value("display/layoutVersion", 0).toInt() < 3) {
        if (m_settings.contains("display/customLayout")) {
            const auto old =
                QJsonDocument::fromJson(m_settings.value("display/customLayout").toByteArray()).toVariant().toList();
            m_settings.setValue("display/customLayout",
                                QJsonDocument::fromVariant(integrateWindDirection(old)).toJson(QJsonDocument::Compact));
        }
        auto saved =
            QJsonDocument::fromJson(m_settings.value("display/savedProfiles").toByteArray()).toVariant().toList();
        for (auto &value : saved) {
            auto entry = value.toMap();
            entry["charts"] = integrateWindDirection(entry["charts"].toList());
            value = entry;
        }
        if (!saved.isEmpty())
            m_settings.setValue("display/savedProfiles",
                                QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        m_settings.setValue("display/layoutVersion", 3);
    }
    if (m_settings.value("places/listVersion", 0).toInt() < 1) {
        auto places = QJsonDocument::fromJson(m_settings.value("places/favorites").toByteArray()).toVariant().toList();
        const auto recent =
            QJsonDocument::fromJson(m_settings.value("places/recent").toByteArray()).toVariant().toList();
        for (const auto &value : recent) {
            const auto candidate = value.toMap();
            bool found = false;
            for (const auto &entry : places) {
                const auto saved = entry.toMap();
                if (std::abs(saved["latitude"].toDouble() - candidate["latitude"].toDouble()) < 0.000001 &&
                    std::abs(saved["longitude"].toDouble() - candidate["longitude"].toDouble()) < 0.000001) {
                    found = true;
                    break;
                }
            }
            if (!found && !candidate["name"].toString().isEmpty())
                places.append(candidate);
        }
        m_settings.setValue("places/favorites", QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
        m_settings.setValue("places/listVersion", 1);
    }
    m_settings.sync();
}
