#pragma once
#include <QJsonDocument>
#include <QVariantList>

namespace Meteo {
QVariantList parseLocations(const QJsonDocument &document);
}
