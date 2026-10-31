#pragma once
#include "MeteoUiExport.hpp"
#include "network/JsonRequest.hpp"
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT ReverseGeocoder : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
public:
    explicit ReverseGeocoder(QObject *parent = nullptr,
        QUrl endpoint = QUrl(QStringLiteral("https://photon.dev.meteo.pl/reverse")));
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    Q_INVOKABLE void lookup(double latitude, double longitude);
    Q_INVOKABLE void cancel();
    static QString placeName(const QJsonDocument &document);
signals:
    void resolved(const QString &name);
    void stateChanged();
private:
    Meteo::JsonRequest m_request;
    QUrl m_endpoint;
    bool m_busy = false;
    QString m_error;
};
