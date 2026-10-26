#include "WebLink.hpp"

QUrl Meteo::resolveWebLink(const QUrl &link, const QUrl &base)
{
    if (link.isEmpty()) return {};
    const auto origin=base.isRelative() ? QUrl(QStringLiteral("https://www.meteo.pl/")).resolved(base) : base;
    const auto resolved=origin.resolved(link);
    return resolved.isValid() && !resolved.host().isEmpty() && (resolved.scheme()=="http" || resolved.scheme()=="https")
        ? resolved : QUrl();
}
