#pragma once
#include <QVariantList>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace DrawingPolicy {
inline QVector<qreal> dashPattern(const QVariantList &input)
{
    QVector<qreal> result;
    for (const auto &entry : input.mid(0,64)) {
        const auto value=entry.toDouble();
        if (std::isfinite(value) && value>0) result.append(std::clamp(value,0.5,100.0));
    }
    if (result.size()%2) result+=result;
    return result;
}
}
