#include "PreferencesStore.hpp"
#include "ChartConfiguration.hpp"
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

namespace
{
QString settingsPath()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("meteo-pl"), QStringLiteral("desktop"))
        .fileName();
}
QString settingsTemporaryFileTemplate()
{
    // Android applications cannot write to the shared system temporary directory.
    // Keep scratch files outside the watched settings and clearable data cache.
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/meteo-pl-preferences");
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("settings-XXXXXX.ini"));
}
// QSettings still owns the INI encoding. A private temporary file avoids its
// shared, process-local cache and pending writes leaking out of a failed edit.
bool readSettings(const QString &path, QVariantMap &values, QString *error)
{
    QFile input(path);
    if (!input.exists()) {
        values.clear();
        return true;
    }
    if (!input.open(QIODevice::ReadOnly)) {
        if (error)
            *error = input.errorString();
        return false;
    }
    QTemporaryFile temporary(settingsTemporaryFileTemplate());
    if (!temporary.open()) {
        if (error)
            *error = temporary.errorString();
        return false;
    }
    const auto bytes = input.readAll();
    if (input.error() != QFile::NoError || temporary.write(bytes) != bytes.size() || !temporary.flush()) {
        if (error)
            *error = Meteo::i18n("Could not read preferences.");
        return false;
    }
    QSettings settings(temporary.fileName(), QSettings::IniFormat);
    QVariantMap result;
    for (const auto &key : settings.allKeys())
        result.insert(key, settings.value(key));
    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = Meteo::i18n("The preferences file is invalid.");
        return false;
    }
    values = std::move(result);
    return true;
}
bool writeSettings(const QString &path, const QVariantMap &values, QString *error)
{
    QTemporaryFile temporary(settingsTemporaryFileTemplate());
    if (!temporary.open()) {
        if (error)
            *error = temporary.errorString();
        return false;
    }
    temporary.close();
    {
        QSettings settings(temporary.fileName(), QSettings::IniFormat);
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            settings.setValue(it.key(), it.value());
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            if (error)
                *error = Meteo::i18n("Could not prepare preferences for saving.");
            return false;
        }
    }
    QFile encoded(temporary.fileName());
    if (!encoded.open(QIODevice::ReadOnly)) {
        if (error)
            *error = encoded.errorString();
        return false;
    }
    const auto bytes = encoded.readAll();
    if (encoded.error() != QFile::NoError) {
        if (error)
            *error = encoded.errorString();
        return false;
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        if (error)
            *error = output.errorString();
        return false;
    }
    return true;
}
} // namespace

PreferencesStore::PreferencesStore(QObject *parent) : QObject(parent), m_fileName(settingsPath())
{
    readSettings(m_fileName, m_values, nullptr);
    update([this] { migrate(); });
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
QVariantList PreferencesStore::savedPlacesSnapshot()
{
    QVariantMap values;
    if (!readSettings(settingsPath(), values, nullptr))
        return {};
    return QJsonDocument::fromJson(values.value("places/favorites").toByteArray()).toVariant().toList();
}
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
    if (!readSettings(m_fileName, values, nullptr))
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
        if (!readSettings(m_fileName, m_pending, error))
            return false;
        const auto original = m_pending;
        m_updating = true;
        const auto reset = qScopeGuard([this] {
            m_updating = false;
            m_pending.clear();
        });
        operation();
        if (m_pending != original && !writeSettings(m_fileName, m_pending, error))
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
void PreferencesStore::migrate()
{
    if (value("display/layoutVersion", 0).toInt() < 2) {
        setValue("display/profile", "classic");
        setValue("display/layoutVersion", 2);
    }
    if (value("display/layoutVersion", 0).toInt() < 3) {
        if (contains("display/customLayout")) {
            const auto old = QJsonDocument::fromJson(value("display/customLayout").toByteArray()).toVariant().toList();
            setValue("display/customLayout",
                     QJsonDocument::fromVariant(integrateWindDirection(old)).toJson(QJsonDocument::Compact));
        }
        auto saved = QJsonDocument::fromJson(value("display/savedProfiles").toByteArray()).toVariant().toList();
        for (auto &value : saved) {
            auto entry = value.toMap();
            entry["charts"] = integrateWindDirection(entry["charts"].toList());
            value = entry;
        }
        if (!saved.isEmpty())
            setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        setValue("display/layoutVersion", 3);
    }
    if (value("display/layoutVersion", 0).toInt() < 4) {
        if (contains("display/customLayout")) {
            const auto old = QJsonDocument::fromJson(value("display/customLayout").toByteArray()).toVariant().toList();
            setValue("display/customLayout",
                     QJsonDocument::fromVariant(removeUnusedChartTemplates(old)).toJson(QJsonDocument::Compact));
        }
        auto saved = QJsonDocument::fromJson(value("display/savedProfiles").toByteArray()).toVariant().toList();
        for (auto &value : saved) {
            auto entry = value.toMap();
            entry["charts"] = removeUnusedChartTemplates(entry["charts"].toList());
            value = entry;
        }
        if (!saved.isEmpty())
            setValue("display/savedProfiles", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
        setValue("display/layoutVersion", 4);
    }
    if (value("places/listVersion", 0).toInt() < 1) {
        auto places = QJsonDocument::fromJson(value("places/favorites").toByteArray()).toVariant().toList();
        const auto recent = QJsonDocument::fromJson(value("places/recent").toByteArray()).toVariant().toList();
        for (const auto &value : recent) {
            const auto candidate = value.toMap();
            bool found = false;
            for (const auto &entry : places) {
                const auto saved = entry.toMap();
                if (Meteo::samePlace(saved["latitude"].toDouble(), saved["longitude"].toDouble(),
                                     candidate["latitude"].toDouble(), candidate["longitude"].toDouble())) {
                    found = true;
                    break;
                }
            }
            if (!found && !candidate["name"].toString().isEmpty())
                places.append(candidate);
        }
        setValue("places/favorites", QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
        setValue("places/listVersion", 1);
    }
    if (value("places/listVersion", 0).toInt() < 2) {
        auto places = QJsonDocument::fromJson(value("places/favorites").toByteArray()).toVariant().toList();
        for (auto &entry : places) {
            auto place = entry.toMap();
            // Existing Plasma selections use these coordinate keys. Retain
            // them as stable IDs when migrating so moving a place keeps it selected.
            if (place.value("id").toString().isEmpty())
                place["id"] = QString::number(place["latitude"].toDouble(), 'f', 6) + ','
                    + QString::number(place["longitude"].toDouble(), 'f', 6);
            entry = place;
        }
        setValue("places/favorites", QJsonDocument::fromVariant(places).toJson(QJsonDocument::Compact));
        setValue("places/listVersion", 2);
    }
}
