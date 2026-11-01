#include "ForecastLauncher.hpp"
#include "core/Forecast.hpp"
#include <QProcess>
#include <QStandardPaths>

bool Meteo::launchForecast(const QString &name, double latitude, double longitude, const QString &model)
{
    const auto executable = QStandardPaths::findExecutable(QStringLiteral("meteo-pl"));
    if (executable.isEmpty())
        return false;
    QStringList arguments;
    if (!name.trimmed().isEmpty()) {
        if (!Meteo::validLocation(latitude, longitude, model))
            return false;
        arguments = {QStringLiteral("--location"), name, QStringLiteral("--latitude"), QString::number(latitude, 'g', 17),
                     QStringLiteral("--longitude"), QString::number(longitude, 'g', 17), QStringLiteral("--model"), model};
    }
    return QProcess::startDetached(executable, arguments);
}
