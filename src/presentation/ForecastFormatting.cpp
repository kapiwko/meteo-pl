#include "ForecastFormatting.hpp"
#include "core/ForecastSampling.hpp"
#include "core/Localization.hpp"
#include <QLocale>
#include <cmath>

QVariantMap formatForecastSample(const Meteo::Forecast &forecast,
                                 const ForecastField &field, qint64 timestamp,
                                 double scale, const QString &windUnit,
                                 const QTimeZone &zone) {
  const auto sample = Meteo::sampleForecast(forecast, field.key, timestamp);
  const auto value = sample ? sample->value : std::nullopt;
  const auto unit = field.unit == "m/s" ? windUnit : field.unit;
  QString text = value
                     ? QLocale().toString(*value * scale, 'f', field.decimals) +
                           (unit.isEmpty() ? QString() : ' ' + unit)
                     : QStringLiteral("—");
  if (field.kind == "direction" && value) {
    const QStringList directions{"N",  "NNE", "NE", "ENE", "E",  "ESE",
                                 "SE", "SSE", "S",  "SSW", "SW", "WSW",
                                 "W",  "WNW", "NW", "NNW"};
    text += " · " + directions[(qRound(*value / 22.5) % 16 + 16) % 16];
  }
  if (field.kind == "type" && value) {
    const int code = qRound(*value);
    text = code == 0   ? Meteo::i18n("None")
           : code == 1 ? Meteo::i18n("Rain")
           : code == 2 ? Meteo::i18n("Snow")
           : code == 3 ? Meteo::i18n("Sleet")
                       : Meteo::i18n("Code %1").arg(code);
  }
  const bool instant =
      sample && sample->timing == Meteo::QuantityTiming::Instant;
  const auto format = [&zone](qint64 time, const QString &pattern) {
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(time, zone),
                              pattern);
  };
  QString sampleText, intervalLabel;
  if (sample) {
    if (instant) {
      sampleText = Meteo::i18n("Sample: %1")
                       .arg(format(sample->sourceTimestamp, "d MMM HH:mm t"));
    } else {
      sampleText = Meteo::i18n("Interval: %1–%2")
                       .arg(format(sample->intervalFrom, "d MMM HH:mm t"),
                            format(sample->intervalTo, "d MMM HH:mm t"));
      const auto from =
          QDateTime::fromSecsSinceEpoch(sample->intervalFrom, zone);
      const auto to = QDateTime::fromSecsSinceEpoch(sample->intervalTo, zone);
      const auto pattern = from.offsetFromUtc() != to.offsetFromUtc()
                               ? "d MMM HH:mm t"
                           : from.date() != to.date() ? "d MMM HH:mm"
                                                      : "HH:mm";
      intervalLabel = format(sample->intervalFrom, pattern) + "–" +
                      format(sample->intervalTo, pattern);
    }
  }
  return {{"key", field.key},
          {"label", field.label},
          {"value", text},
          {"group", field.group},
          {"number", value ? QVariant(*value * scale) : QVariant()},
          {"unit", unit},
          {"color", field.color},
          {"darkColor", forecastDarkColor(field.color, field.key)},
          {"sourceTimestamp",
           sample ? QVariant(sample->sourceTimestamp) : QVariant()},
          {"intervalFrom",
           sample && !instant ? QVariant(sample->intervalFrom) : QVariant()},
          {"intervalTo",
           sample && !instant ? QVariant(sample->intervalTo) : QVariant()},
          {"intervalLabel", intervalLabel},
          {"sampleText", sampleText},
          {"sampling", Meteo::timingName(Meteo::quantityTiming(field.key))}};
}
