#include "Forecast.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Meteo {
namespace {
qint64 timestampValue(const QJsonValue &value)
{
    bool ok = false;
    const auto result = value.isString() ? value.toString().toLongLong(&ok) : value.toInteger(-1);
    return (value.isString() && !ok) ? -1 : result;
}
} // namespace

std::optional<double> Series::at(qint64 timestamp) const
{
    if (interval <= 0 || timestamp < firstTimestamp || values.isEmpty())
        return std::nullopt;
    const auto index = (timestamp - firstTimestamp) / interval;
    if (index >= values.size())
        return std::nullopt;
    return values.at(index);
}

qint64 Forecast::firstTimestamp() const
{
    return series.value(QStringLiteral("airtmp_point")).firstTimestamp;
}

qint64 Forecast::lastTimestamp() const
{
    const auto &temperature = series[QStringLiteral("airtmp_point")];
    return temperature.firstTimestamp + qint64(temperature.interval) * (temperature.values.size() - 1);
}

bool knownModel(const QString &model)
{
    return model == "um4_60" || model == "um4_120" || model == "gfs";
}

bool validLocation(double latitude, double longitude, const QString &model)
{
    if (!knownModel(model) || !std::isfinite(latitude) || !std::isfinite(longitude)
        || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
        return false;
    return model == "gfs" || (latitude >= 44.5 && latitude <= 66.482 && longitude >= 8.21 && longitude <= 30.176);
}

QString modelLabel(const QString &model)
{
    if (model == "um4_120")
        return QStringLiteral("UM4 · 120 h");
    if (model == "gfs")
        return QStringLiteral("GFS · 240 h");
    return QStringLiteral("UM4 · 60 h");
}

std::optional<Forecast> parseForecast(const QByteArray &json, QString *error)
{
    const auto fail = [error](const QString &message) -> std::optional<Forecast> {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (json.size() > 2 * 1024 * 1024)
        return fail(QStringLiteral("Odpowiedź serwera jest zbyt duża."));

    // The upstream occasionally serializes missing numbers as bare nan tokens.
    QString normalized = QString::fromUtf8(json);
    normalized.replace(QRegularExpression(QStringLiteral("([\\[:,]\\s*)(?:nan|-?infinity)(?=\\s*[,\\]}])"),
                                         QRegularExpression::CaseInsensitiveOption), QStringLiteral("\\1null"));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(normalized.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("Niepoprawny format odpowiedzi meteo.pl."));

    const auto root = document.object();
    const auto data = root.value("data").toObject();
    Forecast forecast;
    forecast.modelRun = QDateTime::fromString(root.value("fstart").toString(), Qt::ISODate);
    for (auto it = data.begin(); it != data.end(); ++it) {
        // GFS includes null entries for parameters available only in UM.
        if (it.value().isNull()) continue;
        const auto field = it.value().toObject();
        const auto values = field.value("data").toArray();
        const auto timestamp = timestampValue(field.value("first_timestamp"));
        const int interval = field.value("interval").toInt();
        if (timestamp <= 0 || interval <= 0 || interval > 86400 || values.isEmpty() || values.size() > 1000)
            return fail(QStringLiteral("Niepoprawna oś czasu prognozy."));
        Series series{field.value("unit").toString(), timestamp, interval, {}};
        for (const auto &value : values) {
            if (value.isNull()) {
                series.values.append(std::nullopt);
            } else if (value.isDouble() && std::isfinite(value.toDouble())) {
                // Missing cloud heights are also encoded as large negative
                // sentinels, rather than JSON null, in the current service.
                if ((it.key().startsWith("cldbse") || it.key() == "cldtop") && value.toDouble() < -1000)
                    series.values.append(std::nullopt);
                else series.values.append(value.toDouble());
            } else {
                return fail(QStringLiteral("Niepoprawne wartości prognozy."));
            }
        }
        if (!forecast.modelRun.isValid())
            forecast.modelRun = QDateTime::fromString(field.value("fstart").toString(), Qt::ISODate);
        forecast.series.insert(it.key(), std::move(series));
    }
    const auto temperature = forecast.series.constFind("airtmp_point");
    if (temperature == forecast.series.cend() || !forecast.modelRun.isValid()
        || std::none_of(temperature->values.begin(), temperature->values.end(), [](const auto &v) { return v.has_value(); }))
        return fail(QStringLiteral("Brak temperatury lub daty uruchomienia modelu."));
    return forecast;
}

std::optional<qint64> latestRun(const QByteArray &json, const QString &model)
{
    if (!knownModel(model))
        return std::nullopt;
    const auto values = QJsonDocument::fromJson(json).object().value(model).toArray();
    qint64 newest = 0;
    for (const auto &value : values)
        newest = std::max(newest, timestampValue(value));
    return newest > 0 ? std::optional<qint64>(newest) : std::nullopt;
}
} // namespace Meteo
