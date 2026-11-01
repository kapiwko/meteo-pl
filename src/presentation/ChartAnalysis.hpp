#pragma once
#include "MeteoUiExport.hpp"
#include "ChartMath.hpp"
#include <QObject>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT ChartAnalysis : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit ChartAnalysis(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE QVariantMap axis(double low, double high, double step) const;
    Q_INVOKABLE QVariantMap niceAxis(double low, double high) const;
    Q_INVOKABLE QVariantMap precipitationAxis(double maximum) const;
    Q_INVOKABLE QVariantMap windAxis(double maximum, double scale) const;
    Q_INVOKABLE QVariantMap humidityAxis(double low, double high) const;
    Q_INVOKABLE QString precipitationCaption(const QVariantList &fields) const;
    Q_INVOKABLE QVariantList prepareFields(const QVariantList &fields) const;
};
