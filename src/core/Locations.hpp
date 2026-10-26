#pragma once
#include <QJsonDocument>
#include <QVariantList>

namespace Meteo {
bool samePlace(double latitudeA, double longitudeA, double latitudeB, double longitudeB);
QVariantList parseLocations(const QJsonDocument &document);
}
