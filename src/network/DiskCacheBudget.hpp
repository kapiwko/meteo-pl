#pragma once
#include <QStringList>

namespace Meteo {
struct DiskCacheBudget { qint64 bytes; qsizetype entries; };
inline constexpr DiskCacheBudget forecastCacheBudget{128*1024*1024,256};
inline constexpr DiskCacheBudget editorialCacheBudget{32*1024*1024,256};
inline constexpr DiskCacheBudget metadataCacheBudget{32*1024*1024,512};

// Modification time is an access stamp only; freshness remains in each JSON
// envelope. The current usable entry is retained first within the same budget.
void touchCacheEntry(const QString &path);
void pruneDiskCache(const QString &directory, const QStringList &filters, DiskCacheBudget budget, const QString &currentEntry);
}
