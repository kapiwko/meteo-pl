#include "data/PreferencesFile.hpp"
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class PreferencesFileTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_cache;
private slots:
    void initTestCase()
    {
        QVERIFY(m_cache.isValid());
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        QStandardPaths::setTestModeEnabled(true);
#endif
        qputenv("XDG_CACHE_HOME",m_cache.path().toUtf8());
    }
    void writesAndReplacesAnIniSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path=directory.filePath("nested/preferences.ini");
        QString error;
        const QByteArray firstJson="{\"charts\":[\"temperature\"]}";
        QVERIFY2(Meteo::writePreferencesFile(path,{{"display/renderer","cpu"},
            {"display/layout",firstJson},{"obsolete","remove me"}},&error),qPrintable(error));
        QVariantMap values;
        QVERIFY2(Meteo::readPreferencesFile(path,values,&error),qPrintable(error));
        QCOMPARE(values.value("display/renderer").toString(),QString("cpu"));
        QCOMPARE(values.value("display/layout").toByteArray(),firstJson);
        const QByteArray secondJson="{\"charts\":[\"wind\"]}";
        QVERIFY2(Meteo::writePreferencesFile(path,{{"display/renderer","gpu"},
            {"display/layout",secondJson}},&error),qPrintable(error));
        QVERIFY2(Meteo::readPreferencesFile(path,values,&error),qPrintable(error));
        QCOMPARE(values.value("display/renderer").toString(),QString("gpu"));
        QCOMPARE(values.value("display/layout").toByteArray(),secondJson);
        QVERIFY(!values.contains("obsolete"));
    }
    void missingFilesReturnAnEmptySnapshot()
    {
        QTemporaryDir directory;
        QVariantMap values{{"old","value"}};
        QVERIFY(Meteo::readPreferencesFile(directory.filePath("missing.ini"),values));
        QVERIFY(values.isEmpty());
    }
    void emptySnapshotsClearExistingValues()
    {
        QTemporaryDir directory;
        const auto path=directory.filePath("preferences.ini");
        QString error;
        QVERIFY2(Meteo::writePreferencesFile(path,{{"renderer","cpu"}},&error),qPrintable(error));
        QVERIFY2(Meteo::writePreferencesFile(path,{},&error),qPrintable(error));
        QVariantMap values{{"old","value"}};
        QVERIFY2(Meteo::readPreferencesFile(path,values,&error),qPrintable(error));
        QVERIFY(values.isEmpty());
    }
    void malformedFilesLeaveTheLastSnapshotIntact()
    {
        QTemporaryDir directory;
        const auto path=directory.filePath("invalid.ini");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[unterminated\n"); file.close();
        QVariantMap values{{"old","value"}};
        QString error;
        QVERIFY(!Meteo::readPreferencesFile(path,values,&error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(values,QVariantMap({{"old","value"}}));
    }
    void failedReplacementPreservesOtherFiles()
    {
        QTemporaryDir directory;
        const auto path=directory.filePath("keep.txt");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("keep"); file.close();
        QString error;
        QVERIFY(!Meteo::writePreferencesFile(directory.path(),{{"renderer","cpu"}},&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(),QByteArray("keep"));
    }
};
QTEST_GUILESS_MAIN(PreferencesFileTest)
#include "test_preferences_file.moc"
