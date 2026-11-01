#pragma once
#include <QVariantMap>
#include <QVariantList>
namespace Meteo {
QString preferencesFilePath();
bool readPreferencesFile(const QString &path,QVariantMap &values,QString *error=nullptr);
bool writePreferencesFile(const QString &path,const QVariantMap &values,QString *error=nullptr);
QVariantList savedPlacesSnapshot();
}
