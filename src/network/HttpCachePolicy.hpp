#pragma once
#include <QByteArray>
#include <QDateTime>
#include <chrono>
class QNetworkReply;

namespace Meteo {
bool allowsCacheReuse(const QByteArray &header);
// Explicit HTTP freshness can shorten the application's maximum lifetime.
QDateTime cacheExpiration(const QNetworkReply *reply, const QDateTime &now, qint64 lifetime,
                          std::chrono::milliseconds responseDelay={}, qint64 fallbackLifetime=-1);
}
