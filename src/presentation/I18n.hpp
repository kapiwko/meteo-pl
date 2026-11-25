#pragma once
#include "MeteoUiExport.hpp"
#include "core/Localization.hpp"
#include "core/NumberFormatting.hpp"
#include <QObject>
#include <QQmlEngine>

class METEO_UI_EXPORT I18n : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit I18n(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE QString formatNumber(double value, int decimals = 0) const { return Meteo::formatNumber(value, decimals); }
    Q_INVOKABLE QString i18n(const QString &message) const { return Meteo::i18n(message.toUtf8().constData()); }
    Q_INVOKABLE QString i18np(const QString &singular, const QString &plural, int count) const
    { return Meteo::i18np(singular.toUtf8().constData(), plural.toUtf8().constData(), count); }
};
