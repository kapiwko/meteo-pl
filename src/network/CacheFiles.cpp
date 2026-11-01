#include "CacheFiles.hpp"
#include <QDir>
#include <QFile>

namespace Meteo {
bool isCacheRedirect(const QFileInfo &entry) { return entry.isSymLink() || entry.isJunction(); }
bool cachePathHasRedirects(QString path)
{
    path = QFileInfo(path).absoluteFilePath();
    for (;;) {
        if (isCacheRedirect(QFileInfo(path))) return true;
        const auto parent = QFileInfo(path).absolutePath();
        if (parent == path) return false;
        path = parent;
    }
}
bool cacheTreeHasRedirects(const QString &root)
{
    if (isCacheRedirect(QFileInfo(root))) return true;
    for (const auto &entry : QDir(root).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        if (isCacheRedirect(entry) || (entry.isDir() && cacheTreeHasRedirects(entry.absoluteFilePath()))) return true;
    }
    return false;
}
bool visitCacheFiles(const QString &root, const std::function<bool(const QFileInfo &)> &visit)
{
    if (isCacheRedirect(QFileInfo(root))) return false;
    bool ok = true;
    for (const auto &entry : QDir(root).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        if (isCacheRedirect(entry)) continue;
        const bool accepted = entry.isDir() ? visitCacheFiles(entry.absoluteFilePath(), visit) : visit(entry);
        ok = accepted && ok;
    }
    return ok;
}
bool clearCacheFiles(const QString &root)
{
    // Retain directories and redirect entries, including NTFS junctions.
    return visitCacheFiles(root, [](const QFileInfo &file) { return QFile::remove(file.absoluteFilePath()); });
}
}
