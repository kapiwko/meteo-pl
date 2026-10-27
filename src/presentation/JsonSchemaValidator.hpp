#pragma once
#include "MeteoUiExport.hpp"
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>

METEO_UI_EXPORT bool validateChartJson(const QJsonValue &value, QString *error = nullptr);
QJsonDocument bundledChartJson(const QString &path);
