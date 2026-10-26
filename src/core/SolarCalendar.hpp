#pragma once
#include <QDate>
#include <QTimeZone>
#include <QVector>
#include <optional>

namespace Meteo {
struct TimeSpan { qint64 start, end; };
struct SolarEvents {
    std::optional<qint64> sunrise, sunset;
    bool polarNight = false;
};
// UTC timestamps and geographic coordinates; independent of the display zone.
bool nightAt(qint64 timestamp, double latitude, double longitude);
QVector<TimeSpan> nightsBetween(qint64 from, qint64 to, double latitude, double longitude);
QVector<TimeSpan> calendarSpans(qint64 from, qint64 to, const QTimeZone &zone);
SolarEvents solarEvents(const QDate &day, const QTimeZone &zone, double latitude, double longitude);
}
