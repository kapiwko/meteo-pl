#pragma once
#include "MeteoUiExport.hpp"
#include "network/JsonRequest.hpp"
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT WeatherLayersModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList layers READ layers NOTIFY changed)
    Q_PROPERTY(QVariantList runs READ runs NOTIFY changed)
    Q_PROPERTY(int layerIndex READ layerIndex WRITE setLayerIndex NOTIFY changed)
    Q_PROPERTY(int runIndex READ runIndex WRITE setRunIndex NOTIFY changed)
    Q_PROPERTY(int step READ step WRITE setStep NOTIFY changed)
    Q_PROPERTY(int firstStep READ firstStep NOTIFY changed)
    Q_PROPERTY(int lastStep READ lastStep NOTIFY changed)
    Q_PROPERTY(QString timeLabel READ timeLabel NOTIFY changed)
    Q_PROPERTY(QUrl legendUrl READ legendUrl NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    explicit WeatherLayersModel(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://mapy.meteo.pl")));
    QString model() const { return m_model; }
    bool active() const { return m_active; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QVariantList layers() const;
    QVariantList runs() const { return m_runs; }
    int layerIndex() const { return m_layerIndex; }
    int runIndex() const { return m_runIndex; }
    int step() const { return m_step; }
    int firstStep() const;
    int lastStep() const;
    QString timeLabel() const;
    QUrl legendUrl() const;
    int revision() const { return m_revision; }
    void setModel(const QString &value);
    void setActive(bool value);
    void setLayerIndex(int value);
    void setRunIndex(int value);
    void setStep(int value);
    Q_INVOKABLE QUrl tileUrl(int x, int y, int zoom) const;
    Q_INVOKABLE QUrl tileUrlAtStep(int x, int y, int zoom, int step) const;
    Q_INVOKABLE void reload(bool force = true);
private:
    QString layerId() const;
    void publish();
    Meteo::JsonRequest m_request;
    QString m_model = QStringLiteral("um4_60"), m_error;
    QVariantList m_runs;
    bool m_active = false, m_busy = false;
    int m_layerIndex = 0, m_runIndex = 0, m_step = 1, m_revision = 0;
    QUrl m_endpoint;
signals:
    void changed();
};
