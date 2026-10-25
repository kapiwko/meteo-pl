#pragma once
#include <QtCore/qglobal.h>

#if defined(METEO_UI_BUILD)
#define METEO_UI_EXPORT Q_DECL_EXPORT
#else
#define METEO_UI_EXPORT Q_DECL_IMPORT
#endif
