#include "PreferencesFile.hpp"
#include "core/Localization.hpp"
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QJsonDocument>

namespace Meteo
{
QString preferencesFilePath()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("meteo-pl"), QStringLiteral("desktop"))
        .fileName();
}
QString settingsTemporaryDirectoryTemplate()
{
    // Android applications cannot write to the shared system temporary directory.
    // Keep scratch files outside the watched settings and clearable data cache.
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/meteo-pl-preferences");
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("settings-XXXXXX"));
}
// QSettings still owns the INI encoding. A private temporary file avoids its
// shared, process-local cache and pending writes leaking out of a failed edit.
bool readPreferencesFile(const QString &path, QVariantMap &values, QString *error)
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
    QTemporaryDir temporary(settingsTemporaryDirectoryTemplate());
    if (!temporary.isValid()) {
        if (error)
            *error = temporary.errorString();
        return false;
    }
    const auto temporaryPath = temporary.filePath(QStringLiteral("settings.ini"));
    QFile snapshot(temporaryPath);
    if (!snapshot.open(QIODevice::WriteOnly)) {
        if (error) *error = snapshot.errorString();
        return false;
    }
    const auto bytes = input.readAll();
    if (input.error() != QFile::NoError || snapshot.write(bytes) != bytes.size() || !snapshot.flush()) {
        if (error)
            *error = Meteo::i18n("Could not read preferences.");
        return false;
    }
    snapshot.close();
    QSettings settings(temporaryPath, QSettings::IniFormat);
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
bool writePreferencesFile(const QString &path, const QVariantMap &values, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error=Meteo::i18n("Could not create the preferences directory.");
        return false;
    }
    // Let QSettings own the scratch file inside a private directory. A
    // QTemporaryFile can retain a Windows handle that prevents atomic replacement.
    QTemporaryDir temporary(settingsTemporaryDirectoryTemplate());
    if (!temporary.isValid()) {
        if (error)
            *error = temporary.errorString();
        return false;
    }
    const auto temporaryPath = temporary.filePath(QStringLiteral("settings.ini"));
    // An empty QSettings snapshot does not create a file by itself.
    {
        QFile initial(temporaryPath);
        if (!initial.open(QIODevice::WriteOnly)) {
            if (error) *error = initial.errorString();
            return false;
        }
    }
    {
        QSettings settings(temporaryPath, QSettings::IniFormat);
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            settings.setValue(it.key(), it.value());
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            if (error)
                *error = Meteo::i18n("Could not prepare preferences for saving.");
            return false;
        }
    }
    QFile encoded(temporaryPath);
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


QVariantList Meteo::savedPlacesSnapshot()
{
    QVariantMap values;
    if (!readPreferencesFile(preferencesFilePath(),values,nullptr)) return {};
    return QJsonDocument::fromJson(values.value("places/favorites").toByteArray()).toVariant().toList();
}
