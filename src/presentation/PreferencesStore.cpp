#include "PreferencesStore.hpp"
#include "data/PreferencesFile.hpp"
#include "PreferencesMigration.hpp"
#include "core/Localization.hpp"
#include "core/Locations.hpp"
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryFile>

PreferencesStore::PreferencesStore(QObject *parent) : QObject(parent), m_fileName(Meteo::preferencesFilePath())
{
    Meteo::readPreferencesFile(m_fileName, m_values, nullptr);
    update([this] { migratePreferences(m_pending); });
    m_places = placesSnapshot();
    m_display = displaySnapshot();
    watchFile();
    m_reload.setSingleShot(true);
    m_reload.setInterval(40);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_reload.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reload.start(); });
    connect(&m_reload, &QTimer::timeout, this, [this] { sync(); });
}
void PreferencesStore::watchFile()
{
    const auto directory = QFileInfo(m_fileName).absolutePath();
    if (!m_watcher.directories().contains(directory) && QDir(directory).exists())
        m_watcher.addPath(directory);
    if (!m_watcher.files().contains(m_fileName) && QFile::exists(m_fileName))
        m_watcher.addPath(m_fileName);
}
QVariantList PreferencesStore::savedPlacesSnapshot() { return Meteo::savedPlacesSnapshot(); }
QVariant PreferencesStore::value(const QString &key, const QVariant &fallback) const
{
    return (m_updating ? m_pending : m_values).value(key, fallback);
}
bool PreferencesStore::contains(const QString &key) const { return (m_updating ? m_pending : m_values).contains(key); }
void PreferencesStore::setValue(const QString &key, const QVariant &value)
{
    if (m_updating)
        m_pending.insert(key, value);
    else
        update([&] { m_pending.insert(key, value); });
}
void PreferencesStore::remove(const QString &key)
{
    if (m_updating)
        m_pending.remove(key);
    else
        update([&] { m_pending.remove(key); });
}
QString PreferencesStore::fileName() const { return m_fileName; }
bool PreferencesStore::sync()
{
    if (m_updating)
        return true;
    QVariantMap values;
    if (!Meteo::readPreferencesFile(m_fileName, values, nullptr))
        return false;
    publish(std::move(values));
    return true;
}
bool PreferencesStore::update(const std::function<void()> &operation, QString *error)
{
    if (m_updating) {
        operation();
        return true;
    }
    QVariantMap committed;
    {
        const auto directory = QFileInfo(m_fileName).absolutePath();
        if (!QDir().mkpath(directory)) {
            if (error)
                *error = Meteo::i18n("Could not create the preferences directory.");
            return false;
        }
        // Separate from QSettings' own .lock: cover the whole read/edit/write.
        QLockFile lock(m_fileName + QStringLiteral(".mutation.lock"));
        if (!lock.tryLock(3000)) {
            if (error)
                *error = Meteo::i18n("Preferences are being changed by another process. Try again.");
            return false;
        }
        if (!Meteo::readPreferencesFile(m_fileName, m_pending, error))
            return false;
        const auto original = m_pending;
        m_updating = true;
        const auto reset = qScopeGuard([this] {
            m_updating = false;
            m_pending.clear();
        });
        operation();
        if (m_pending != original && !Meteo::writePreferencesFile(m_fileName, m_pending, error))
            return false;
        committed = m_pending;
    }
    // Never invoke views or start another edit while holding the mutation lock.
    publish(std::move(committed));
    return true;
}
void PreferencesStore::publish(QVariantMap values)
{
    m_values = std::move(values);
    const auto places = placesSnapshot(), display = displaySnapshot();
    const bool placesUpdated = places != m_places, displayUpdated = display != m_display;
    m_places = places;
    m_display = display;
    watchFile();
    if (placesUpdated)
        emit placesChanged();
    if (displayUpdated)
        emit displayChanged();
}
QByteArray PreferencesStore::placesSnapshot() const
{
    return value("places/favorites").toByteArray() + '\n' + value("places/recent").toByteArray();
}
QByteArray PreferencesStore::displaySnapshot() const
{
    QVariantMap values;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        if (it.key().startsWith("display/"))
            values.insert(it.key(), it.value());
    QByteArray snapshot;
    QDataStream stream(&snapshot, QIODevice::WriteOnly);
    stream << values;
    return snapshot;
}
