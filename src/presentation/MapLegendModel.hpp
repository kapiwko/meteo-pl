#pragma once
#include "MeteoUiExport.hpp"
#include "network/JsonRequest.hpp"
#include <QJsonObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT MapLegendModel : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString layer READ layer WRITE setLayer NOTIFY changed)
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QVariantList legends READ legends NOTIFY changed)
public:
  explicit MapLegendModel(QObject *parent = nullptr,
                          QUrl endpoint = QUrl("https://mapy.meteo.pl"));
  QString layer() const { return m_layer; }
  bool active() const { return m_active; }
  bool busy() const { return m_pending > 0; }
  QString error() const { return m_error; }
  QVariantList legends() const;
  void setLayer(const QString &layer);
  void setActive(bool active);
  Q_INVOKABLE void reload(bool force = true);
signals:
  void changed();

private:
  void fail(const QString &error);
  void accepted();
  Meteo::JsonRequest m_stylesRequest, m_layersRequest;
  QUrl m_endpoint;
  QJsonObject m_styles, m_layers, m_incomingStyles, m_incomingLayers;
  QString m_layer, m_error;
  bool m_active = false;
  int m_pending = 0;
};
