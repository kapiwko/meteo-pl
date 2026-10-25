#include "DiskCacheBudget.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <algorithm>

namespace Meteo {
void touchCacheEntry(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) file.setFileTime(QDateTime::currentDateTimeUtc(),QFileDevice::FileModificationTime);
}
void pruneDiskCache(const QString &directory, const QStringList &filters, DiskCacheBudget budget, const QString &currentEntry)
{
    auto files = QDir(directory).entryInfoList(filters,QDir::Files|QDir::NoSymLinks,QDir::Time);
    const auto current = QFileInfo(currentEntry).absoluteFilePath();
    std::stable_sort(files.begin(),files.end(),[&](const QFileInfo &a,const QFileInfo &b) {
        return a.absoluteFilePath() == current && b.absoluteFilePath() != current;
    });
    qint64 bytes = 0;
    qsizetype entries = 0;
    for (const auto &file : std::as_const(files)) {
        if (entries >= budget.entries || file.size() > budget.bytes-bytes) QFile::remove(file.absoluteFilePath());
        else { bytes += file.size(); ++entries; }
    }
}
}
