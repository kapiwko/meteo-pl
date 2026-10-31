#include "ApplicationTheme.hpp"
#include <QGuiApplication>
#include <QStyleHints>
#ifdef METEO_NATIVE_COLOR_SCHEMES
#include <KColorSchemeManager>
#include <QModelIndex>
#endif

void ApplicationTheme::setMode(const QString &mode)
{
    if (m_mode == mode || (mode != "automatic" && mode != "light" && mode != "dark")) return;
    m_mode = mode;
    if (m_ready) apply();
    emit modeChanged();
}
void ApplicationTheme::apply()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(m_mode == "dark" ? Qt::ColorScheme::Dark
        : m_mode == "light" ? Qt::ColorScheme::Light : Qt::ColorScheme::Unknown);
#endif
#ifdef METEO_NATIVE_COLOR_SCHEMES
    auto *manager = KColorSchemeManager::instance();
    manager->setAutosaveChanges(false);
    manager->activateScheme(manager->indexForSchemeId(m_mode == "automatic" ? QString{}
        : m_mode == "dark" ? QStringLiteral("BreezeDark") : QStringLiteral("BreezeLight")));
#endif
}
