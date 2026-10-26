#pragma once
#include <QUrl>

namespace Meteo {
QUrl resolveWebLink(const QUrl &link, const QUrl &base = QUrl(QStringLiteral("https://www.meteo.pl/")));
}
