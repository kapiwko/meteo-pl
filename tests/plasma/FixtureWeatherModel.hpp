#pragma once
#include "WeatherModel.hpp"
#include "Fixture.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>
#include <QtQml/qqmlregistration.h>

// Test-only endpoint injection. All forecast/cache/state handling stays in the
// production WeatherModel, with synthetic data from the shared test fixture.
class FixtureWeatherModel : public WeatherModel {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit FixtureWeatherModel(QObject *parent = nullptr)
        : WeatherModel(parent, QUrl(qEnvironmentVariable("METEO_TEST_FORECAST_ENDPOINT"))) {}
};

class FixtureDriver final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString scenario READ scenario CONSTANT)
    Q_PROPERTY(QString screenshot READ screenshot CONSTANT)
public:
    using QObject::QObject;
    QString scenario() const { return qEnvironmentVariable("METEO_TEST_SCENARIO"); }
    QString screenshot() const { return qEnvironmentVariable("METEO_TEST_SCREENSHOT"); }
    Q_INVOKABLE void prepare()
    {
        auto fixture = QJsonDocument::fromJson(forecastFixture()).object();
        auto fields = fixture["data"].toObject();
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            auto field = it.value().toObject();
            const auto original = field["data"].toArray(); QJsonArray values;
            for (int i = 0; i < 12; ++i) for (const auto &value : original) values.append(value);
            field["data"] = values; it.value() = field;
        }
        fixture["data"] = fields;
        m_payload = QJsonDocument(fixture).toJson();
        QFile file(qEnvironmentVariable("METEO_TEST_PAYLOAD"));
        if (file.open(QIODevice::WriteOnly)) file.write(m_payload);
    }
    Q_INVOKABLE void seedCache(WeatherModel *weather)
    {
        const auto identity = weather->model() + ':' + QString::number(weather->latitude(), 'f', 6) + ':' + QString::number(weather->longitude(), 'f', 6);
        const auto hash = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
        const auto directory = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/meteo-pl";
        QDir().mkpath(directory);
        QFile file(directory + '/' + hash + ".json");
        if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{
            {"model", weather->model()}, {"latitude", weather->latitude()}, {"longitude", weather->longitude()},
            {"savedAt", QDateTime::currentDateTimeUtc().addSecs(-7200).toString(Qt::ISODate)},
            {"payload", QString::fromUtf8(m_payload)}}).toJson());
    }
    Q_INVOKABLE void keyClick(QQuickItem *item, int key)
    {
        if (!item || !item->window()) return;
        item->forceActiveFocus(); item->window()->requestActivate();
        QTest::keyClick(item->window(), Qt::Key(key));
    }
    Q_INVOKABLE bool capture(QQuickItem *item)
    {
        if (!item || !item->window()) return false;
        // Capture the host window too: the native representation inherits its
        // background from Plasma, so an item-only grab would be transparent.
        return item->window()->grabWindow().save(screenshot());
    }
    Q_INVOKABLE void report(bool passed, const QString &message)
    {
        qInfo().noquote() << (passed ? "METEO_TEST_PASS" : "METEO_TEST_FAIL") << scenario() << message;
    }
private:
    QByteArray m_payload;
};
