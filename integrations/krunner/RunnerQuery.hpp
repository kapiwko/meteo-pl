#pragma once
#include <QString>
#include <optional>

namespace Meteo {
std::optional<QString> runnerQuery(const QString &query);
QString searchablePlaceName(const QString &name);
}
