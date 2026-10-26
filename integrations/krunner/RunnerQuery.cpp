#include "RunnerQuery.hpp"
#include <QRegularExpression>

std::optional<QString> Meteo::runnerQuery(const QString &query)
{
    if (query.size() > 256) return std::nullopt;
    static const QRegularExpression pattern(QStringLiteral("^(?:pogoda|weather|meteo)(?:\\s+(.*))?$"), QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(query.simplified());
    return match.hasMatch() ? std::optional<QString>(match.captured(1)) : std::nullopt;
}
QString Meteo::searchablePlaceName(const QString &name)
{
    static const QRegularExpression accents(QStringLiteral("\\p{M}"));
    return name.toCaseFolded().normalized(QString::NormalizationForm_D).remove(accents).replace(QChar(0x0142), QChar('l'));
}
