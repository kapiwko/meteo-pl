#include "HttpCachePolicy.hpp"
#include <QNetworkReply>
#include <QDateTime>
#include <QRegularExpression>
#include <QLocale>
#include <QTimeZone>
#include <algorithm>
#include <chrono>

namespace Meteo {
bool allowsCacheReuse(const QByteArray &header)
{
    for (auto directive : header.toLower().split(',')) {
        directive=directive.trimmed();
        if (directive=="no-store" || directive=="no-cache" || directive.startsWith("no-cache=")) return false;
    }
    static const QRegularExpression zeroAge(QStringLiteral(R"((?:^|,)\s*max-age\s*=\s*"?0"?\s*(?:,|$))"));
    return !zeroAge.match(QString::fromLatin1(header.toLower())).hasMatch();
}
// Explicit HTTP freshness can shorten the application's maximum lifetime.
QDateTime cacheExpiration(const QNetworkReply *reply, const QDateTime &now, qint64 lifetime,
                                std::chrono::milliseconds responseDelay, qint64 fallbackLifetime)
{
    if (lifetime<=0) return {};
    const auto control=reply->rawHeader("Cache-Control").toLower();
    if (!allowsCacheReuse(control)) return {};
    static const QRegularExpression maxAge(QStringLiteral(R"((?:^|,)\s*max-age\s*=\s*"?(\d+))"));
    const auto match=maxAge.match(QString::fromLatin1(control));
    if (match.hasMatch()) {
        bool ok=false; const auto seconds=match.captured(1).toLongLong(&ok);
        if (ok) {
            const auto age=std::max<qint64>(0,reply->rawHeader("Age").toLongLong());
            // RFC 9111 section 4.2.3: corrected_age_value includes the time
            // from sending the request until receiving the response.
            const auto expiry=now.addSecs(std::max<qint64>(0,seconds-age))
                .addMSecs(-std::max<qint64>(0,responseDelay.count()));
            return expiry.isValid() && expiry>now ? std::min(now.addSecs(lifetime),expiry) : QDateTime{};
        }
    } else {
        const auto text=QString::fromLatin1(reply->rawHeader("Expires"));
        auto expires=QDateTime::fromString(text,Qt::RFC2822Date);
        if (!expires.isValid()) {
            expires=QLocale::c().toDateTime(text,QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
            if (expires.isValid()) expires.setTimeZone(QTimeZone::UTC);
        }
        if (expires.isValid()) lifetime=std::min(lifetime,now.secsTo(expires));
        else if (fallbackLifetime>=0) lifetime=std::min(lifetime,fallbackLifetime);
    }
    return lifetime>0 ? now.addSecs(lifetime) : QDateTime{};
}
}
