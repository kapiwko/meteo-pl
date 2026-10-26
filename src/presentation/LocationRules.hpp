#pragma once
#include "MeteoUiExport.hpp"
#include "core/Forecast.hpp"
#include "core/Locations.hpp"
#include <QObject>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT LocationRules : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    using QObject::QObject;
    Q_INVOKABLE bool samePlace(double latitudeA, double longitudeA, double latitudeB, double longitudeB) const
    { return Meteo::samePlace(latitudeA, longitudeA, latitudeB, longitudeB); }
    Q_INVOKABLE bool valid(double latitude, double longitude, const QString &model) const
    { return Meteo::validLocation(latitude, longitude, model); }
    Q_INVOKABLE QString modelFor(double latitude, double longitude, const QString &preferred) const
    {
        if (Meteo::validLocation(latitude, longitude, preferred)) return preferred;
        return Meteo::validLocation(latitude, longitude, QStringLiteral("gfs")) ? QStringLiteral("gfs") : QString();
    }
};
