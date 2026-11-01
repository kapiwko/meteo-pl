#include "ForecastAggregation.hpp"
#include <algorithm>
#include <cmath>
#include <QSet>

namespace Meteo {
namespace {
struct DayWork {
    DailyForecast day;
    double cloudWeighted = 0;
    qint64 cloudSeconds = 0;
    bool missingRain = false;
};
template<typename Function> void samples(const Forecast &forecast, const QString &key, Function function)
{
    const auto found = forecast.series.constFind(key);
    if (found == forecast.series.cend() || found->interval <= 0) return;
    for (qsizetype i=0; i<found->values.size(); ++i) {
        if (found->firstTimestamp <= 0 || found->firstTimestamp > 253402300799LL-qint64(i+1)*found->interval) break;
        function(found->firstTimestamp+qint64(i)*found->interval, found->interval, found->values[i]);
    }
}
template<typename Function> void calendarParts(qint64 from, int interval, const QTimeZone &zone, Function function)
{
    const auto end = from+interval;
    const bool split = QDateTime::fromSecsSinceEpoch(from, zone).date() != QDateTime::fromSecsSinceEpoch(end-1, zone).date();
    for (int part=0; from<end && part<4; ++part) {
        const auto date = QDateTime::fromSecsSinceEpoch(from, zone).date();
        const auto next = date.addDays(1).startOfDay(zone).toSecsSinceEpoch();
        if (!date.isValid() || next <= from) break;
        const auto until = std::min(end, next);
        function(date, until-from, split);
        from = until;
    }
}
void minimum(std::optional<double> &target, double value) { target = target ? std::min(*target,value) : value; }
void maximum(std::optional<double> &target, double value) { target = target ? std::max(*target,value) : value; }
}

QVector<DailyForecast> aggregateDays(const Forecast &forecast, const QTimeZone &zone)
{
    QMap<QDate, DayWork> days;
    const auto dayAt = [&](qint64 timestamp) -> DayWork & {
        const auto date = QDateTime::fromSecsSinceEpoch(timestamp,zone).date();
        auto &work = days[date]; work.day.date = date; return work;
    };
    for (const auto &key : {QStringLiteral("airtmp_point"),QStringLiteral("airtmp_min"),QStringLiteral("airtmp_max")}) {
        samples(forecast,key,[&](qint64 time,int,const std::optional<double> &value) {
            auto &day = dayAt(time).day;
            if (!value) return;
            if (key != "airtmp_max") minimum(day.minimum,*value);
            if (key != "airtmp_min") maximum(day.maximum,*value);
        });
    }
    samples(forecast,"wind_gust_max",[&](qint64 time,int interval,const std::optional<double> &value) {
        if (!value) return;
        calendarParts(time,interval,zone,[&](QDate date,qint64,bool split) {
            auto &day=days[date].day; day.date=date;
            maximum(day.gust,*value); day.gustEstimatedAtBoundary |= split;
        });
    });
    samples(forecast,"pcpttl_aver",[&](qint64 time,int interval,const std::optional<double> &value) {
        calendarParts(time,interval,zone,[&](QDate date,qint64 seconds,bool split) {
            auto &work = days[date]; work.day.date = date;
            if (!value) { work.missingRain = true; return; }
            work.day.rain = work.day.rain.value_or(0) + *value * (double(seconds)/interval);
            work.day.rainCoverageSeconds += seconds;
            work.day.rainEstimatedAtBoundary |= split;
            if (*value>0) {
                const auto from=std::max(time,date.startOfDay(zone).toSecsSinceEpoch());
                const auto to=std::min(time+interval,date.addDays(1).startOfDay(zone).toSecsSinceEpoch());
                QSet<int> categories; qint64 covered=0;
                samples(forecast,"pcpttl_type_max",[&](qint64 start,int duration,const std::optional<double> &type) {
                    const auto overlap=std::min(to,start+duration)-std::max(from,start);
                    if (overlap<=0 || !type) return;
                    categories.insert(qRound(*type)); covered+=overlap;
                });
                work.day.snow |= categories.contains(2);
                work.day.mixed |= categories.contains(3);
                work.day.precipitationTypeUncertain |= categories.size()>1 || covered<to-from;
            }
        });
    });
    samples(forecast,"cldtot_aver",[&](qint64 time,int interval,const std::optional<double> &value) {
        if (!value) return;
        calendarParts(time,interval,zone,[&](QDate date,qint64 seconds,bool) {
            auto &work = days[date]; work.day.date = date;
            work.cloudWeighted += *value*seconds;
            work.cloudSeconds += seconds;
        });
    });
    for (const auto &key : {QStringLiteral("storm_max"),QStringLiteral("flash_max")}) {
        samples(forecast,key,[&](qint64 time,int interval,const std::optional<double> &value) {
            if (!value || *value <= 0) return;
            calendarParts(time,interval,zone,[&](QDate date,qint64,bool) { days[date].day.date=date; days[date].day.storm=true; });
        });
    }
    QVector<DailyForecast> result;
    for (auto &work : days) {
        if (!work.day.date.isValid()) continue;
        const auto duration = work.day.date.startOfDay(zone).secsTo(work.day.date.addDays(1).startOfDay(zone));
        work.day.rainComplete = !work.missingRain && work.day.rainCoverageSeconds == duration;
        if (work.cloudSeconds > 0) work.day.cloudCover = work.cloudWeighted/work.cloudSeconds;
        result.append(work.day);
    }
    return result;
}
}
