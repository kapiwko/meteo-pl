#pragma once
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>

bool validateChartJson(const QJsonValue &value, QString *error = nullptr);
QJsonDocument bundledChartJson(const QString &path);
