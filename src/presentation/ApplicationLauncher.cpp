#include "ApplicationLauncher.hpp"
#include "data/ForecastLauncher.hpp"
bool ApplicationLauncher::openForecast(const QString &name,double latitude,double longitude,const QString &model) const
{ return Meteo::launchForecast(name,latitude,longitude,model); }
