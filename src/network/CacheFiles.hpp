#pragma once
#include <QFileInfo>
#include <functional>

namespace Meteo {
bool isCacheRedirect(const QFileInfo &entry);
bool cachePathHasRedirects(QString path);
bool cacheTreeHasRedirects(const QString &root);
bool visitCacheFiles(const QString &root, const std::function<bool(const QFileInfo &)> &visit);
bool clearCacheFiles(const QString &root);
}
