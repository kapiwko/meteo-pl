#pragma once
#include <QLocale>
#include <cmath>

namespace Meteo {
inline QString formatNumber(double value, int decimals = 0, const QLocale &locale = QLocale())
{
    const auto text = locale.toString(value, 'f', decimals);
    if (std::signbit(value)) {
        // Use the formatted value to respect the locale's rounding and precision.
        bool valid = false;
        if (locale.toDouble(text, &valid) == 0 && valid)
            return locale.toString(0.0, 'f', decimals);
    }
    return text;
}
}
