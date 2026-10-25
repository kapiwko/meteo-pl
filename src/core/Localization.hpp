#pragma once
#include <QString>

namespace Meteo {
// A fixed domain keeps translations independent from the Plasma host application.
QString i18n(const char *message);
QString i18np(const char *singular, const char *plural, int count);
}
