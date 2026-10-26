#include "MapLegendModel.hpp"
#include "core/Localization.hpp"
#include <QColor>
#include <QJsonArray>
#include <QLocale>
#include <utility>

MapLegendModel::MapLegendModel(QObject *parent, QUrl endpoint)
    : QObject(parent), m_endpoint(std::move(endpoint)) {
  connect(&m_stylesRequest, &Meteo::JsonRequest::received, this,
          [this](const QJsonDocument &document) {
            m_incomingStyles = document.object().value("styles").toObject();
            accepted();
          });
  connect(&m_layersRequest, &Meteo::JsonRequest::received, this,
          [this](const QJsonDocument &document) {
            m_incomingLayers = document.object();
            accepted();
          });
  for (auto *request : {&m_stylesRequest, &m_layersRequest})
    connect(request, &Meteo::JsonRequest::failed, this, &MapLegendModel::fail);
}
void MapLegendModel::accepted() {
  if (--m_pending == 0) {
    m_styles = m_incomingStyles;
    m_layers = m_incomingLayers;
  }
  emit changed();
}
void MapLegendModel::fail(const QString &error) {
  m_stylesRequest.cancel();
  m_layersRequest.cancel();
  m_pending = 0;
  m_error = error;
  emit changed();
}
void MapLegendModel::setLayer(const QString &layer) {
  if (m_layer == layer)
    return;
  m_layer = layer;
  emit changed();
}
void MapLegendModel::setActive(bool active) {
  if (m_active == active)
    return;
  m_active = active;
  if (active && !busy() && (m_styles.isEmpty() || m_layers.isEmpty()))
    reload(false);
  else
    emit changed();
}
void MapLegendModel::reload(bool force) {
  m_stylesRequest.cancel();
  m_layersRequest.cancel();
  m_pending = 2;
  m_error.clear();
  emit changed();
  m_stylesRequest.get(m_endpoint.resolved(QUrl("/json/legend_style.json")),
                      86400, force, [](const QJsonDocument &document) {
                        return document.isObject() &&
                               document.object().value("styles").isObject();
                      });
  m_layersRequest.get(m_endpoint.resolved(QUrl("/json/layers_legend.json")),
                      86400, force, [](const QJsonDocument &document) {
                        return document.isObject() &&
                               !document.object().isEmpty();
                      });
}
QVariantList MapLegendModel::legends() const {
  QVariantList result;
  const auto layer =
      m_layer == "um:UM4_WIND" ? QStringLiteral("um:UM4_wind_vect") : m_layer;
  for (const auto &entry : m_layers.value(layer).toArray()) {
    const auto key = entry.toString();
    QVariantList stops;
    const auto values = m_styles.value(key).toArray();
    if (values.size() > 256)
      continue;
    for (const auto &value : values) {
      const auto object = value.toObject();
      const QColor color(object.value("color").toString());
      auto label = object.value("label").toString();
      if (!color.isValid() || label.size() > 64)
        continue;
      if (label == "very low")
        label = Meteo::i18n("Very low clouds");
      else if (label == "low")
        label = Meteo::i18n("Low clouds");
      else if (label == "medium")
        label = Meteo::i18n("Medium clouds");
      else if (label == "high clouds")
        label = Meteo::i18n("High clouds");
      else {
        bool numeric = false;
        const auto number = label.toDouble(&numeric);
        if (numeric)
          label = QLocale().toString(number, 'g', 8);
      }
      stops.append(QVariantMap{{"color", color.name(QColor::HexArgb)},
                               {"label", label}});
    }
    if (stops.isEmpty())
      continue;
    QString title = key, unit;
    if (key.startsWith("tempC")) {
      title = Meteo::i18n("Temperature");
      unit = "°C";
    } else if (key == "pressure_hPa") {
      title = Meteo::i18n("Pressure");
      unit = "hPa";
    } else if (key == "wind") {
      title = Meteo::i18n("Wind");
      unit = "m/s";
    } else if (key == "clouds")
      title = Meteo::i18n("Clouds");
    else if (key.startsWith("rain_")) {
      title = Meteo::i18n("Rain");
      unit = key.endsWith("6h") ? "mm / 6 h" : "mm / 1 h";
    } else if (key.startsWith("snow_")) {
      title = Meteo::i18n("Snow");
      unit = key.endsWith("6h") ? Meteo::i18n("mm of water / 6 h")
                                : Meteo::i18n("mm of water / 1 h");
    }
    result.append(QVariantMap{{"key", key},
                              {"title", title},
                              {"unit", unit},
                              {"stops", stops},
                              {"categorical", key == "clouds"}});
  }
  return result;
}
