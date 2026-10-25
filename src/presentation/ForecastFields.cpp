#include "core/Localization.hpp"
#include "ForecastFields.hpp"
#include <QHash>

const QVector<ForecastField> &forecastFields()
{
    static const QVector<ForecastField> fields{
        // Line colors from app.meteo.pl's light meteogram palette (25.10.2026).
        {"airtmp_point", Meteo::i18n("Air temperature"), "°C", "temperature", "#f44336", "line"},
        {"airtmp_min", Meteo::i18n("Minimum temperature"), "°C", "temperature", "#fadcdc", "lower"},
        {"airtmp_max", Meteo::i18n("Maximum temperature"), "°C", "temperature", "#fadcdc", "upper"},
        {"dwptmp_point", Meteo::i18n("Dew point"), "°C", "temperature", "#6a5acd", "points"},
        {"wchill_point", Meteo::i18n("Feels like temperature"), "°C", "temperature", "#00008b", "line"},
        {"wchill_min", Meteo::i18n("Minimum feels like"), "°C", "temperature", "#b9dcff", "hidden"},
        {"wchill_max", Meteo::i18n("Maximum feels like"), "°C", "temperature", "#b9dcff", "hidden"},
        {"grdtmp_min", Meteo::i18n("Near ground · minimum"), "°C", "ground", "#795548", "line"},
        {"grdtmp_max", Meteo::i18n("Near ground · maximum"), "°C", "ground", "#795548", "line"},
        {"pcpttl_aver", Meteo::i18n("Mean precipitation in interval"), "mm", "precipitation", "#00aa00", "bars"},
        {"pcpttl_max", Meteo::i18n("Maximum precipitation in interval"), "mm", "precipitation", "#00aa00", "stems"},
        {"pcpttlprob_point", Meteo::i18n("Probability of precipitation"), "%", "precipitation", "#00aa00", "right", 1, 0},
        {"pcpttl_type_max", Meteo::i18n("Precipitation type"), "", "precipitation", "#00aa00", "type", 1, 0},
        {"wind10_sd_true_prev_point", Meteo::i18n("Wind at 10 m"), "m/s", "wind", "#111199", "line"},
        {"wind_gust_max", Meteo::i18n("Wind gusts"), "m/s", "wind", "#f44336", "line"},
        {"wind10_dr_deg_true_prev_point", Meteo::i18n("Wind direction · from"), "°", "wind", "#111199", "direction", 1, 0},
        {"slpres_point", Meteo::i18n("Sea level pressure"), "hPa", "pressure", "#000000", "line", 0.01, 0},
        {"trpres_point", Meteo::i18n("Surface pressure"), "hPa", "pressure", "#000000", "line", 0.01, 0},
        {"cldtot_aver", Meteo::i18n("Total cloud cover"), "%", "clouds", "#dc4614", "line", 100, 0},
        {"cldvlow_aver", Meteo::i18n("Very low clouds"), "%", "clouds", "#424242", "dashed", 100, 0},
        {"cldlow_aver", Meteo::i18n("Low clouds"), "%", "clouds", "#5a5a5a", "line", 100, 0},
        {"cldmed_aver", Meteo::i18n("Medium clouds"), "%", "clouds", "#969696", "line", 100, 0},
        {"cldhigh_aver", Meteo::i18n("High clouds"), "%", "clouds", "#ffffff", "line", 100, 0},
        {"cldbse01", Meteo::i18n("Cloud base · >0.1 octas"), "km", "cloudHeights", "#ffffff", "line"},
        {"cldbse25", Meteo::i18n("Cloud base · >2.5 octas"), "km", "cloudHeights", "#969696", "line"},
        {"cldbse45", Meteo::i18n("Cloud base · >4.5 octas"), "km", "cloudHeights", "#5a5a5a", "line"},
        {"cldbse65", Meteo::i18n("Cloud base · >6.5 octas"), "km", "cloudHeights", "#424242", "line"},
        {"cldbse79", Meteo::i18n("Cloud base · >7.9 octas"), "km", "cloudHeights", "#000000", "line"},
        {"cldtop", Meteo::i18n("Cloud tops"), "km", "cloudHeights", "#8b302c", "dashed"},
        {"realhum_aver", Meteo::i18n("Relative humidity"), "%", "humidity", "#dc4614", "line", 1, 0},
        {"fog_max", Meteo::i18n("Fog cover"), "%", "humidity", "#ffb400", "area", 100, 0},
        {"visibl_min", Meteo::i18n("Minimum visibility"), "km", "visibility", "#ff9600", "line", 0.001},
        {"storm_max", Meteo::i18n("Storm intensity"), "", "storms", "#ff9800", "bars", 1, 0},
        {"flash_max", Meteo::i18n("Lightning indicator"), "", "storms", "#f44336", "right"}
    };
    return fields;
}

QString forecastDarkColor(const QString &lightColor, const QString &fieldKey)
{
    if (fieldKey == "cldbse79") return lightColor;
    if (fieldKey == "cldtot_aver") return lightColor;
    static const QHash<QString, QString> palette{
        {"#f44336", "#ff6b6b"}, {"#fadcdc", "#4d2626"}, {"#6a5acd", "#9f7aea"},
        {"#00008b", "#4a90e2"}, {"#b9dcff", "#1a3a5c"}, {"#795548", "#8b6914"},
        {"#00aa00", "#4ade80"}, {"#111199", "#5b9cf6"}, {"#000000", "#ffffff"},
        {"#dc4614", "#ff8c42"}, {"#424242", "#373737"}, {"#ff9600", "#ff9e40"}
    };
    return palette.value(lightColor, lightColor);
}

const QVector<ForecastChart> &forecastCharts()
{
    static const QVector<ForecastChart> charts{
        {"temperature", Meteo::i18n("Temperature"), {"airtmp_point", "airtmp_min", "airtmp_max", "dwptmp_point", "wchill_point", "wchill_min", "wchill_max", "grdtmp_min", "grdtmp_max"}},
        {"precipitation", Meteo::i18n("Precipitation and humidity"), {"pcpttl_aver", "pcpttl_max", "pcpttl_type_max", "realhum_aver"}},
        {"probability", Meteo::i18n("Precipitation chance and storms"), {"pcpttlprob_point", "storm_max", "flash_max"}},
        {"pressure", Meteo::i18n("Sea level pressure"), {"slpres_point"}},
        {"wind", Meteo::i18n("Wind"), {"wind10_sd_true_prev_point", "wind_gust_max", "wind10_dr_deg_true_prev_point"}},
        {"windDirection", Meteo::i18n("Wind direction"), {"wind10_dr_deg_true_prev_point"}},
        {"cloudHeights", Meteo::i18n("Cloud heights and visibility"), {"cldbse01", "cldbse25", "cldbse45", "cldbse65", "cldbse79", "cldtop", "visibl_min"}},
        {"clouds", Meteo::i18n("Cloud cover and fog"), {"cldhigh_aver", "cldmed_aver", "cldlow_aver", "cldvlow_aver", "cldtot_aver", "fog_max"}},
        {"ground", Meteo::i18n("Near-ground temperature"), {"grdtmp_min", "grdtmp_max"}},
        {"humidity", Meteo::i18n("Humidity and fog"), {"realhum_aver", "fog_max"}},
        {"visibility", Meteo::i18n("Minimum visibility"), {"visibl_min"}},
        {"storms", Meteo::i18n("Storms and lightning"), {"storm_max", "flash_max"}}
    };
    return charts;
}
