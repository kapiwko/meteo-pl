#include "presentation/ChartConfiguration.hpp"
#include "presentation/ProfileFileCodec.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class ProfileCodecTest : public QObject
{
    Q_OBJECT
private slots:
    void roundTripWithoutAnApplicationWindow()
    {
        const QVariantList profiles{
            QVariantMap{{"name", "My charts"}, {"charts", defaultChartLayout("classic")}, {"chartHeight", 220}}};
        QString error;
        const auto bytes = ProfileFileCodec::encode(profiles, &error);
        QVERIFY2(bytes.has_value(), qPrintable(error));
        const auto decoded = ProfileFileCodec::decode(*bytes, &error);
        QVERIFY2(decoded.has_value(), qPrintable(error));
        QCOMPARE(*decoded, profiles);
        QTemporaryDir directory;
        const auto file = QUrl::fromLocalFile(directory.filePath("profiles.json"));
        QVERIFY(ProfileFileCodec::write(file, profiles, &error));
        QCOMPARE(ProfileFileCodec::read(file), decoded);
    }
    void rejectsTheWholeFileIfTheLastProfileIsInvalid()
    {
        const auto charts = QJsonArray::fromVariantList(defaultChartLayout("classic"));
        QJsonObject invalidChart{{"key", "unknown"},
                                 {"series", QJsonArray{QJsonObject{{"key", "not-a-weather-field"}}}}};
        const auto bytes =
            QJsonDocument(QJsonObject{{"format", "pl.meteo.chart-profiles"},
                                      {"version", 1},
                                      {"profiles", QJsonArray{QJsonObject{{"name", "Valid"}, {"charts", charts}},
                                                              QJsonObject{{"name", "Invalid"},
                                                                          {"charts", QJsonArray{invalidChart}}}}}})
                .toJson();
        QString error;
        QVERIFY(!ProfileFileCodec::decode(bytes, &error));
        QVERIFY(!error.isEmpty());
        QTemporaryDir directory;
        const auto path = directory.filePath("profiles.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("existing"), 8);
        file.close();
        QVERIFY(!ProfileFileCodec::write(QUrl::fromLocalFile(path), {QVariantMap{{"name", "Invalid"}}}, &error));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("existing"));
    }
    void boundsInputAndRequiresALocalRegularFile()
    {
        QString error;
        QVERIFY(!ProfileFileCodec::decode(QByteArray(ProfileFileCodec::maximumBytes + 1, ' '), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!ProfileFileCodec::read(QUrl("https://example.org/profiles.json"), &error));
        QTemporaryDir directory;
        QVERIFY(!ProfileFileCodec::read(QUrl::fromLocalFile(directory.path()), &error));
        const auto profile = QVariantMap{{"name", "Profile"}, {"charts", defaultChartLayout("classic")}};
        QVariantList tooMany;
        for (int i = 0; i < 101; ++i)
            tooMany.append(profile);
        QVERIFY(!ProfileFileCodec::encode(tooMany, &error));
    }
};
QTEST_GUILESS_MAIN(ProfileCodecTest)
#include "test_profile_codec.moc"
