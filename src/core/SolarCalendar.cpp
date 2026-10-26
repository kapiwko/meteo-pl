#include "SolarCalendar.hpp"
#include <QDateTime>
#include <cmath>
#include <numbers>

namespace Meteo {
bool nightAt(qint64 timestamp, double latitude, double longitude)
{
    constexpr double rad = std::numbers::pi / 180.0;
    const auto time = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC);
    const double gamma = 2 * std::numbers::pi / 365 * (time.date().dayOfYear() - 1 + (time.time().hour() - 12.0) / 24);
    const double declination = 0.006918 - 0.399912 * std::cos(gamma) + 0.070257 * std::sin(gamma)
        - 0.006758 * std::cos(2 * gamma) + 0.000907 * std::sin(2 * gamma) - 0.002697 * std::cos(3 * gamma) + 0.00148 * std::sin(3 * gamma);
    const double equation = 229.18 * (0.000075 + 0.001868 * std::cos(gamma) - 0.032077 * std::sin(gamma)
        - 0.014615 * std::cos(2 * gamma) - 0.040849 * std::sin(2 * gamma));
    const double minutes = time.time().hour() * 60 + time.time().minute() + equation + 4 * longitude;
    const double hourAngle = (minutes / 4 - 180) * rad;
    const double elevation = std::asin(std::sin(latitude * rad) * std::sin(declination)
        + std::cos(latitude * rad) * std::cos(declination) * std::cos(hourAngle)) / rad;
    return elevation < -0.833;
}
}

QVector<Meteo::TimeSpan> Meteo::nightsBetween(qint64 from, qint64 to, double latitude, double longitude)
{
    QVector<TimeSpan> result;
    if (to <= from || static_cast<long double>(to) - from > 366 * 86400.0L) return result;
    std::optional<qint64> start;
    for (auto time = from;;) {
        const bool night = nightAt(time, latitude, longitude);
        if (night && !start) start = time;
        if (!night && start) { result.append({*start, time}); start.reset(); }
        if (time == to) break;
        time = to - time < 900 ? to : time + 900;
    }
    if (start && *start < to) result.append({*start, to});
    return result;
}
QVector<Meteo::TimeSpan> Meteo::calendarSpans(qint64 from, qint64 to, const QTimeZone &zone)
{
    QVector<TimeSpan> result;
    if (to <= from || static_cast<long double>(to) - from > 366 * 86400.0L || !zone.isValid()) return result;
    auto date = QDateTime::fromSecsSinceEpoch(from, zone).date();
    const auto last = QDateTime::fromSecsSinceEpoch(to - 1, zone).date();
    for (; date.isValid() && date <= last; date = date.addDays(1)) {
        const auto start = date.startOfDay(zone), end = date.addDays(1).startOfDay(zone);
        if (start.isValid() && end.isValid()) result.append({start.toSecsSinceEpoch(), end.toSecsSinceEpoch()});
    }
    return result;
}
Meteo::SolarEvents Meteo::solarEvents(const QDate &day, const QTimeZone &zone, double latitude, double longitude)
{
    SolarEvents result;
    const auto first = day.startOfDay(zone), last = day.addDays(1).startOfDay(zone);
    if (!first.isValid() || !last.isValid()) return result;
    const auto from = first.toSecsSinceEpoch(), to = last.toSecsSinceEpoch();
    bool previous = nightAt(from, latitude, longitude);
    for (auto time = from + 60; time < to; time += 60) {
        const bool night = nightAt(time, latitude, longitude);
        if (previous && !night) result.sunrise = time;
        if (!previous && night) result.sunset = time;
        previous = night;
    }
    result.polarNight = previous;
    return result;
}
