#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QQmlParserStatus>
#include <QtQml/qqmlregistration.h>

// Instantiate only in the application's window, never in a Plasma applet.
class METEO_UI_EXPORT ApplicationTheme : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
public:
    using QObject::QObject;
    QString mode() const { return m_mode; }
    void setMode(const QString &mode);
    void classBegin() override {}
    void componentComplete() override { m_ready = true; apply(); }
signals:
    void modeChanged();
private:
    void apply();
    QString m_mode = QStringLiteral("automatic");
    bool m_ready = false;
};
