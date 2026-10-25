#pragma once
#include "MeteoUiExport.hpp"
#include "network/ThematicMapSource.hpp"
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <functional>

class METEO_UI_EXPORT ThematicMapsModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList catalog READ catalog CONSTANT)
    Q_PROPERTY(int layerIndex READ layerIndex WRITE setLayerIndex NOTIFY changed)
    Q_PROPERTY(int step READ step WRITE setStep NOTIFY changed)
    Q_PROPERTY(double run READ run WRITE setRun NOTIFY changed)
    Q_PROPERTY(bool wave READ wave NOTIFY changed)
    Q_PROPERTY(int stepHours READ stepHours NOTIFY changed)
    Q_PROPERTY(int lastStep READ lastStep NOTIFY changed)
    Q_PROPERTY(int attempts READ attempts NOTIFY changed)
    Q_PROPERTY(bool canGoLater READ canGoLater NOTIFY changed)
    Q_PROPERTY(QUrl imageUrl READ imageUrl NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl endpoint READ endpoint WRITE setEndpoint NOTIFY changed)
public:
    using Clock = std::function<qint64()>;
    explicit ThematicMapsModel(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://old.meteo.pl")),
                               Clock clock = {});
    QVariantList catalog() const;
    int layerIndex() const { return m_layerIndex; }
    int step() const { return m_step; }
    double run() const { return m_run; }
    bool wave() const { return Meteo::ThematicMapSource::layer(m_layerIndex).wave(); }
    int stepHours() const { return Meteo::ThematicMapSource::layer(m_layerIndex).stepHours(); }
    int lastStep() const { return Meteo::ThematicMapSource::layer(m_layerIndex).lastStep(); }
    int attempts() const { return m_attempts; }
    bool canGoLater() const;
    QUrl imageUrl() const { return urlForStep(m_step); }
    QString error() const { return m_error; }
    QUrl endpoint() const { return m_source.endpoint(); }
    void setEndpoint(const QUrl &url);
    void setLayerIndex(int value);
    void setStep(int value);
    void setRun(double value);
    Q_INVOKABLE QUrl urlForStep(int hour) const;
    Q_INVOKABLE int nextStep() const;
    Q_INVOKABLE QVariantList animationUrls() const;
    Q_INVOKABLE void latest();
    Q_INVOKABLE void earlier();
    Q_INVOKABLE void later();
    Q_INVOKABLE void imageFailed(const QUrl &url, const QString &error, int status);
    Q_INVOKABLE void imageReady(const QUrl &url);
signals:
    void changed();
    void reloadRequested();

private:
    Meteo::ThematicMapSource m_source;
    Clock m_clock;
    int m_layerIndex = 0, m_step = 12, m_attempts = 0;
    qint64 m_run = 0;
    QString m_error;
};
