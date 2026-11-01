#pragma once
#include "MeteoUiExport.hpp"
#include "data/EditorialRepository.hpp"
#include "EditorialState.hpp"
#include <QVariantList>
#include <QMap>
#include <QDateTime>
#include <QTimer>
#include <functional>
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
    Q_PROPERTY(QString today READ today NOTIFY changed)
public:
    using Clock = std::function<QDate()>;
    explicit EditorialModel(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://meteo-backend.dev.meteo.pl")), Clock clock = {});
    bool commentary() const { return m_commentary; }
    QString date() const { return m_comment.date; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    QVariantList articles() const { return m_article.entries; }
    QString commentHtml() const { return m_comment.html; }
    QString publishedDate() const { return m_comment.publishedDate; }
    QString today() const { return m_today.toString(Qt::ISODate); }
    void setCommentary(bool value);
    void setDate(const QString &value);
    Q_INVOKABLE void reload(bool force = true);
    Q_INVOKABLE void more();
private slots:
    void updateCalendarDay();
private:
    void load(int page, bool force = false);
    Meteo::EditorialRepository m_repository;
    ArticleNavigation m_article;
    CommentaryNavigation m_comment;
    bool m_commentary = true, m_busy = false;
    QString m_error,m_notice;
    bool m_cached = false;
    Clock m_clock;
    QDate m_today;
    QTimer m_calendarTimer;
signals:
    void changed();
};
