#pragma once
#include <QNetworkReply>
#include <QDateTime>
#include <QRegularExpression>
#include <QLocale>
#include <QTimeZone>
#include <algorithm>

namespace Meteo {
inline bool allowsCacheReuse(const QByteArray &header)
{
    for (auto directive : header.toLower().split(',')) {
        directive=directive.trimmed();
        if (directive=="no-store" || directive=="no-cache" || directive.startsWith("no-cache=")) return false;
    }
    static const QRegularExpression zeroAge(QStringLiteral(R"((?:^|,)\s*max-age\s*=\s*"?0"?\s*(?:,|$))"));
    return !zeroAge.match(QString::fromLatin1(header.toLower())).hasMatch();
}
// Explicit HTTP freshness can shorten the application's maximum lifetime.
inline QDateTime cacheExpiration(const QNetworkReply *reply, const QDateTime &now, qint64 lifetime, qint64 fallbackLifetime=-1)
{
    const auto control=reply->rawHeader("Cache-Control").toLower();
    if (!allowsCacheReuse(control)) return {};
    static const QRegularExpression maxAge(QStringLiteral(R"((?:^|,)\s*max-age\s*=\s*"?(\d+))"));
    const auto match=maxAge.match(QString::fromLatin1(control));
    if (match.hasMatch()) {
        bool ok=false; const auto seconds=match.captured(1).toLongLong(&ok);
        if (ok) {
            const auto age=std::max<qint64>(0,reply->rawHeader("Age").toLongLong());
            lifetime=std::min(lifetime,std::max<qint64>(0,seconds-age));
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
