#include "CacheStorage.hpp"
#include "network/ImageCache.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLocale>
#include <QRunnable>
#include <QStandardPaths>

namespace {
bool removeFiles(const QString &path)
{
    bool ok = true;
    for (const auto &entry : QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        if (entry.isDir() && !entry.isSymLink()) ok = removeFiles(entry.absoluteFilePath()) && ok;
        else ok = QFile::remove(entry.absoluteFilePath()) && ok;
    }
    // Keep directories: other app/widget processes may have an open HTTP cache.
    return ok;
}
}
CacheStorage::CacheStorage(QObject *parent) : QObject(parent) { m_worker.setMaxThreadCount(1); }
QString CacheStorage::directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/meteo-pl");
}
QString CacheStorage::sizeText() const { return QLocale().formattedDataSize(m_size); }
QString CacheStorage::forecastSize() const { return QLocale().formattedDataSize(m_forecasts); }
QString CacheStorage::editorialSize() const { return QLocale().formattedDataSize(m_editorial); }
QString CacheStorage::imageSize() const { return QLocale().formattedDataSize(m_images); }
QString CacheStorage::lookupSize() const { return QLocale().formattedDataSize(m_lookups); }
void CacheStorage::refresh() { inspect(false); }
void CacheStorage::clear() { inspect(true); }
void CacheStorage::inspect(bool remove)
{
    if (m_busy) return;
    const auto path = directory();
    m_error.clear();
    if (QFileInfo(path).isSymLink()) {
        m_error = QStringLiteral("Katalog pamięci podręcznej jest dowiązaniem. Nie można go wyczyścić automatycznie.");
        emit changed();
        return;
    }
    m_busy = true;
    emit changed();
    if (remove) Meteo::ImageCache::shared()->clear();
    m_worker.start(QRunnable::create([this, path, remove] {
        const bool ok = !remove || removeFiles(path);
        qint64 total = 0, forecasts = 0, editorial = 0, images = 0, lookups = 0;
        QDirIterator files(path, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            files.next();
            const auto file = files.fileInfo();
            if (file.isSymLink()) continue;
            const auto size = file.size();
            total += size;
            const auto relative = QDir(path).relativeFilePath(file.absoluteFilePath());
            if (relative.startsWith("maps/")) images += size;
            else if (relative.startsWith("editorial/")) editorial += size;
            else if (relative.startsWith("metadata/")) lookups += size;
            else if (!relative.contains('/') && relative.endsWith(".json")) forecasts += size;
        }
        QMetaObject::invokeMethod(this, [this, total, forecasts, editorial, images, lookups, ok, remove] {
            m_size = total; m_forecasts = forecasts; m_editorial = editorial; m_images = images;
            m_lookups = lookups;
            m_busy = false;
            if (!ok) m_error = QStringLiteral("Nie udało się usunąć wszystkich plików pamięci podręcznej. Spróbuj ponownie.");
            emit changed();
            if (remove && ok) emit cleared();
        }, Qt::QueuedConnection);
    }));
}
