#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT ApplicationLauncher : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    using QObject::QObject;
    Q_INVOKABLE bool openForecast(const QString &name, double latitude, double longitude, const QString &model) const;
};
