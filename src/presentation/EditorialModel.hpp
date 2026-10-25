#pragma once
#include "MeteoUiExport.hpp"
#include "network/JsonRequest.hpp"
#include <QVariantList>
#include <QMap>
#include <QDateTime>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT EditorialModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool commentary READ commentary WRITE setCommentary NOTIFY changed)
    Q_PROPERTY(QString date READ date WRITE setDate NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QVariantList articles READ articles NOTIFY changed)
    Q_PROPERTY(QString commentHtml READ commentHtml NOTIFY changed)
    Q_PROPERTY(QString publishedDate READ publishedDate NOTIFY changed)
public:
    explicit EditorialModel(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://meteo-backend.dev.meteo.pl")));
    bool commentary() const { return m_commentary; }
    QString date() const { return m_date; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    QVariantList articles() const { return m_articles; }
    QString commentHtml() const { return m_commentHtml; }
    QString publishedDate() const { return m_publishedDate; }
    void setCommentary(bool value);
    void setDate(const QString &value);
    Q_INVOKABLE void reload(bool force = true);
    Q_INVOKABLE void more();
private:
    void load(int page, bool force = false);
    void applyDocument(const QJsonDocument &document);
    QString cachePath() const;
    bool loadCache();
    void saveCache(const QJsonDocument &document) const;
    Meteo::JsonRequest m_request;
    bool m_commentary = true, m_busy = false;
    int m_page = 1;
    QString m_date, m_error, m_notice, m_commentHtml, m_publishedDate;
    QVariantList m_articles;
    QMap<int, QVariantList> m_pages;
    QUrl m_url;
    QDateTime m_savedAt;
    bool m_cached = false;
    QUrl m_endpoint;
signals:
    void changed();
};
