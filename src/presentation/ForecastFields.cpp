#include "ForecastFields.hpp"
#include <QHash>

const QVector<ForecastField> &forecastFields()
{
    static const QVector<ForecastField> fields{
        // Line colors from app.meteo.pl's light meteogram palette (25.10.2026).
        {"airtmp_point", "Temperatura powietrza", "°C", "temperature", "#f44336", "line"},
        {"airtmp_min", "Temperatura minimalna", "°C", "temperature", "#edab88", "lower"},
        {"airtmp_max", "Temperatura maksymalna", "°C", "temperature", "#edab88", "upper"},
        {"dwptmp_point", "Punkt rosy", "°C", "temperature", "#6a5acd", "points"},
        {"wchill_point", "Temperatura odczuwalna", "°C", "temperature", "#00008b", "line"},
        {"wchill_min", "Odczuwalna minimalna", "°C", "temperature", "#b9dcff", "hidden"},
        {"wchill_max", "Odczuwalna maksymalna", "°C", "temperature", "#b9dcff", "hidden"},
        {"grdtmp_min", "Przy gruncie · minimum", "°C", "ground", "#368f83", "line"},
        {"grdtmp_max", "Przy gruncie · maksimum", "°C", "ground", "#368f83", "line"},
        {"pcpttl_aver", "Opad średni w przedziale", "mm", "precipitation", "#00aa00", "bars"},
        {"pcpttl_max", "Opad maksymalny w przedziale", "mm", "precipitation", "#00aa00", "stems"},
        {"pcpttlprob_point", "Prawdopodobieństwo opadu", "%", "precipitation", "#00aa00", "right", 1, 0},
        {"pcpttl_type_max", "Rodzaj opadu", "", "precipitation", "#00aa00", "type", 1, 0},
        {"wind10_sd_true_prev_point", "Wiatr na 10 m", "m/s", "wind", "#111199", "line"},
        {"wind_gust_max", "Porywy wiatru", "m/s", "wind", "#f44336", "line"},
        {"wind10_dr_deg_true_prev_point", "Kierunek wiatru · skąd wieje", "°", "wind", "#111199", "direction", 1, 0},
        {"slpres_point", "Ciśnienie na poziomie morza", "hPa", "pressure", "#000000", "line", 0.01, 0},
        {"cldtot_aver", "Zachmurzenie całkowite", "%", "clouds", "#dc4614", "line", 100, 0},
        {"cldvlow_aver", "Chmury bardzo niskie", "%", "clouds", "#424242", "dashed", 100, 0},
        {"cldlow_aver", "Chmury niskie", "%", "clouds", "#5a5a5a", "line", 100, 0},
        {"cldmed_aver", "Chmury średnie", "%", "clouds", "#969696", "line", 100, 0},
        {"cldhigh_aver", "Chmury wysokie", "%", "clouds", "#ffffff", "line", 100, 0},
        {"cldbse01", "Podstawa chmur · >0,1 okt.", "km", "cloudHeights", "#ffffff", "line"},
        {"cldbse25", "Podstawa chmur · >2,5 okt.", "km", "cloudHeights", "#969696", "line"},
        {"cldbse45", "Podstawa chmur · >4,5 okt.", "km", "cloudHeights", "#5a5a5a", "line"},
        {"cldbse65", "Podstawa chmur · >6,5 okt.", "km", "cloudHeights", "#424242", "line"},
        {"cldbse79", "Podstawa chmur · >7,9 okt.", "km", "cloudHeights", "#000000", "line"},
        {"cldtop", "Wierzchołki chmur", "km", "cloudHeights", "#8b302c", "dashed"},
        {"realhum_aver", "Wilgotność względna", "%", "humidity", "#dc4614", "line", 1, 0},
        {"fog_max", "Pokrycie mgłą", "%", "humidity", "#ffb400", "area", 100, 0},
        {"visibl_min", "Widzialność minimalna", "km", "visibility", "#ff9600", "line", 0.001},
        {"storm_max", "Intensywność burzy", "", "storms", "#ff9800", "bars", 1, 0},
        {"flash_max", "Wskaźnik wyładowań", "", "storms", "#f44336", "right"}
    };
    return fields;
}

QString forecastDarkColor(const QString &lightColor)
{
    static const QHash<QString, QString> palette{
        {"#f44336", "#ff6b6b"}, {"#edab88", "#bb8060"}, {"#368f83", "#63c6ad"}, {"#6a5acd", "#9f7aea"},
        {"#00008b", "#4a90e2"}, {"#b9dcff", "#1a3a5c"}, {"#795548", "#8b6914"},
        {"#00aa00", "#4ade80"}, {"#111199", "#5b9cf6"}, {"#000000", "#ffffff"},
        {"#dc4614", "#ff8c42"}, {"#424242", "#373737"}, {"#ff9600", "#ff9e40"}
    };
    return palette.value(lightColor, lightColor);
}

const QVector<ForecastChart> &forecastCharts()
{
    static const QVector<ForecastChart> charts{
        {"temperature", "Temperatura", {"airtmp_point", "airtmp_min", "airtmp_max", "dwptmp_point", "wchill_point", "wchill_min", "wchill_max", "grdtmp_min", "grdtmp_max"}},
        {"precipitation", "Opady i wilgotność", {"pcpttl_aver", "pcpttl_max", "pcpttl_type_max", "realhum_aver"}},
        {"probability", "Szansa opadu i burze", {"pcpttlprob_point", "storm_max", "flash_max"}},
        {"pressure", "Ciśnienie na poziomie morza", {"slpres_point"}},
        {"wind", "Prędkość i porywy wiatru", {"wind10_sd_true_prev_point", "wind_gust_max"}},
        {"windDirection", "Kierunek wiatru", {"wind10_dr_deg_true_prev_point"}},
        {"cloudHeights", "Pionowy zasięg chmur i widzialność", {"cldbse01", "cldbse25", "cldbse45", "cldbse65", "cldbse79", "cldtop", "visibl_min"}},
        {"clouds", "Zachmurzenie i mgła", {"cldhigh_aver", "cldmed_aver", "cldlow_aver", "cldvlow_aver", "cldtot_aver", "fog_max"}},
        {"ground", "Temperatura przy gruncie", {"grdtmp_min", "grdtmp_max"}},
        {"humidity", "Wilgotność i mgła", {"realhum_aver", "fog_max"}},
        {"visibility", "Widzialność minimalna", {"visibl_min"}},
        {"storms", "Burze i wyładowania", {"storm_max", "flash_max"}}
    };
    return charts;
}
