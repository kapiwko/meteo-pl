# ICM chart rendering

The comparison was performed on 2026-10-25 against the
[web client](https://app.meteo.pl/meteogram) and its public
[main.dart.js](https://app.meteo.pl/main.dart.js). Actual responses for Kielce
were checked with UM4 60 h, UM4 120 h and GFS 240 h. Repository tests use
synthetic data only.

| Default chart | Series and rendering | Axes and markers |
| --- | --- | --- |
| Temperature | Red air temperature, dark-blue feels-like temperature, purple dew-point dots; translucent min–max ranges; brown vertical ground-surface min–max stems | °C; 1°C padding around extremes; light-blue background below freezing |
| Precipitation and humidity | Adjacent mean bars, thin maximum stems, continuous humidity line; green rain, blue snow, purple mixed precipitation | mm per forecast interval; 5/10/20/30 mm scale; separate fitted humidity axis; red dot for an off-scale maximum |
| Precipitation chance and storms | Green probability line; storm and lightning markers at interval centers | 0–100%; ring for possible storms; yellow, orange and red for positive, ≥10 and ≥30 lightning counts |
| Pressure | Black line with an ICM color-table fill | hPa and mm Hg; at least a 15 hPa range plus margins |
| Wind speed, gusts and direction | Dark-blue speed line and ICM fill palette; red gust dashes spanning their intervals; direction arrows below the zero baseline, pointing along airflow from the reported source direction | m/s and km/h; scale thresholds 5/10/15/20/25/30/40/50 m/s; gust influence limited to four times maximum speed; off-scale dot; shared time axis |
| Cloud extent and visibility | Cloud-base points from full cover to sparse clouds, brown cloud tops above them, orange visibility line | Nonlinear cloud levels 0/0.5/2/7/15 km; visibility 0/1/5/20/100 km |
| Cloud cover and fog | High, medium, low and very-low cloud fills in order, cover lines, orange total cloud cover, yellow fog stems | 0–8 oktas; fog fraction 0–1 |

Line and fill palettes have light and dark variants matching the web client.
Black overcast cloud-base points remain black in the dark theme. Grid and frame
are drawn above fills; night shading is shared, and cursor markers stay on top.

These seven charts define the Classic ICM profile. Custom charts use the same
format: series, renderer, colors, opacity, lines, points and assignment to two
axes are stored in the profile. Different data types can be combined, with
automatic or explicit axis limits. Toggling a legend series preserves the axis
range. Time-label density adapts to the visible interval and chart width:
minutes disappear first, then label spacing increases. Date labels also avoid
overlap. Dates are centered between midnight lines extended above the plot,
including time-zone and daylight-saving transitions. Units sit next to their
respective axes. The grid retains three-hour spacing with lighter strokes and
frames chosen for this application's appearance.

Maximum precipitation describes a spatial maximum. An off-scale maximum is not
a convection indicator. See the source explanations:
[Reading meteograms](https://www.meteo.pl/jak-czytac-prognoze-pogody-czesc-i-meteogramy)
and [Local precipitation and convection](https://www.meteo.pl/gdzie-ten-deszcz-czyli-dlaczego-prognoza-pogody-sie-nie-sprawdzila).
