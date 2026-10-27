#include "UserPreferences.hpp"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <array>

class PreferencesTest : public QObject {
    Q_OBJECT
private slots:
    void failedCommitPreservesTheLastSnapshotAndAllowsRetry()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        PreferencesStore store;
        store.setValue("display/profile", "basic");
        QSignalSpy changed(&store, &PreferencesStore::displayChanged);
        const auto path = store.fileName(), backup = path + ".backup";
        QString error;
        const bool saved = store.update([&] {
            store.setValue("display/profile", "aviation");
            QVERIFY(QFile::rename(path, backup));
            QVERIFY(QDir().mkdir(path)); // Deterministic failure even with root permissions.
        }, &error);
        QVERIFY(QDir().rmdir(path));
        QVERIFY(QFile::rename(backup, path));
        QVERIFY(!saved);
        QVERIFY(!error.isEmpty());
        QCOMPARE(store.value("display/profile").toString(), "basic");
        QCOMPARE(changed.count(), 0);
        PreferencesStore reader;
        QCOMPARE(reader.value("display/profile").toString(), "basic");
        QVERIFY(store.update([&] { store.setValue("display/profile", "aviation"); }, &error));
        QCOMPARE(changed.count(), 1);
        QVERIFY(reader.sync());
        QCOMPARE(reader.value("display/profile").toString(), "aviation");
    }
    void importReportsFailedPersistenceAndCanBeRetried()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        UserPreferences preferences;
        const auto file = QUrl::fromLocalFile(config.filePath("profile.json"));
        QVERIFY(preferences.exportProfiles(file)["ok"].toBool());
        const auto before = preferences.profiles();
        const auto selected = preferences.profile();
        QSignalSpy errorChanged(&preferences, &UserPreferences::errorChanged);
        QLockFile blocker(preferences.settingsFile() + ".mutation.lock");
        QVERIFY(blocker.tryLock());
        const auto result = preferences.importProfiles(file);
        QVERIFY(!result["ok"].toBool());
        QVERIFY(!result["error"].toString().isEmpty());
        QCOMPARE(result["error"].toString(), preferences.error());
        QCOMPARE(preferences.profiles(), before);
        QCOMPARE(preferences.profile(), selected);
        QCOMPARE(errorChanged.count(), 1);
        blocker.unlock();
        QVERIFY(preferences.importProfiles(file)["ok"].toBool());
        QCOMPARE(preferences.profiles().size(), before.size() + 1);
        QVERIFY(preferences.profile().startsWith("user:"));
        QVERIFY(preferences.error().isEmpty());
        QCOMPARE(errorChanged.count(), 2);
    }
    void concurrentMutationsPreserveBothProcesses_data()
    {
        QTest::addColumn<QString>("operation");
        for (const auto &operation : {"places", "profiles", "edits", "rename-add", "remove-save", "save-remove"})
            QTest::newRow(operation) << QString::fromLatin1(operation);
    }
    void concurrentMutationsPreserveBothProcesses()
    {
        QFETCH(QString, operation);
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        UserPreferences preferences;
        preferences.savePlace("Original", 50, 20);
        const QStringList profiles{preferences.createProfile("First"), preferences.createProfile("Second")};
        QVERIFY(!profiles[0].isEmpty() && !profiles[1].isEmpty());
        std::array<QProcess, 2> workers;
        for (int i = 0; i < 2; ++i)
            workers[i].start(QCoreApplication::applicationFilePath(),
                {"--mutate", config.path(), operation, QString::number(i), profiles[i]});
        for (int i = 0; i < 2; ++i) {
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(config.filePath(QString::number(i) + ".ready")), 10000);
        }
        // Both processes have read the same collections. Their watchers cannot
        // run while they wait: each edit must reload under the mutation lock.
        QFile go(config.filePath("go"));
        QVERIFY(go.open(QIODevice::WriteOnly)); go.close();
        for (auto &worker : workers) {
            QVERIFY(worker.waitForFinished(15000));
            QCOMPARE(worker.exitCode(), 0);
        }
        UserPreferences persisted;
        if (operation == "places") {
            QCOMPARE(persisted.favorites().size(), 3);
            QCOMPARE(persisted.recentPlaces().size(), 2);
        } else if (operation == "profiles") {
            QCOMPARE(persisted.profiles().size(), 10); // Six presets, two original and two concurrent profiles.
        } else if (operation == "edits") {
            for (int i = 0; i < 2; ++i)
                QCOMPARE(persisted.chartLayoutForProfile(profiles[i]).first().toMap()["title"].toString(),
                         "Edited " + QString::number(i));
        } else if (operation == "rename-add") {
            QCOMPARE(persisted.favorites().size(), 2);
            QCOMPARE(persisted.placeAt(50, 20)["name"].toString(), "Renamed");
            QVERIFY(persisted.isFavorite(51, 21));
        } else {
            // Same-place conflicts follow the last explicit committed operation.
            QCOMPARE(persisted.isFavorite(50, 20), operation == "remove-save");
            if (operation == "remove-save") QCOMPARE(persisted.placeAt(50, 20)["name"].toString(), "Renamed");
        }
    }
    void anotherProcessUpdatesExistingQmlBindings()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml
            import pl.meteo.desktop
            QtObject {
                property string renderer: UserPreferences.renderer
                property string unit: UserPreferences.windUnit
                property bool utc: UserPreferences.utc
                property bool legend: UserPreferences.showLegend
                property int chartHeight: UserPreferences.chartHeight
                property string profile: UserPreferences.profile
                property int places: UserPreferences.places.length
            }
        )", {});
        QScopedPointer<QObject> bindings(component.create());
        QVERIFY2(bindings, qPrintable(component.errorString()));
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(preferences);
        QCOMPARE(bindings->property("renderer").toString(), "gpu");
        QSignalSpy display(preferences, &UserPreferences::displayChanged);
        QSignalSpy places(preferences, &UserPreferences::placesChanged);
        QProcess writer;
        writer.start(QCoreApplication::applicationFilePath(), {"--write-settings", preferences->settingsFile()});
        QVERIFY(writer.waitForFinished());
        QCOMPARE(writer.exitCode(), 0);
        QTRY_COMPARE(bindings->property("renderer").toString(), "cpu");
        QCOMPARE(bindings->property("unit").toString(), "km/h");
        QCOMPARE(bindings->property("utc").toBool(), true);
        QCOMPARE(bindings->property("legend").toBool(), true);
        QCOMPARE(bindings->property("chartHeight").toInt(), 262);
        QCOMPARE(bindings->property("profile").toString(), "full");
        QCOMPARE(bindings->property("places").toInt(), 1);
        QCOMPARE(display.count(), 1);
        QCOMPARE(places.count(), 1);
        preferences->setRenderer("gpu");
        QTest::qWait(150);
        QCOMPARE(display.count(), 2); // No duplicate from watching our own atomic write.
        QCOMPARE(places.count(), 1);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().value(1) == "--mutate") {
        const auto arguments = application.arguments();
        const auto directory = arguments.value(2), operation = arguments.value(3);
        const int worker = arguments.value(4).toInt();
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
        UserPreferences preferences;
        if (operation == "edits") preferences.setProfile(arguments.value(5));
        QFile ready(directory + "/" + QString::number(worker) + ".ready");
        if (!ready.open(QIODevice::WriteOnly)) return 2;
        ready.close();
        QDeadlineTimer deadline(10000);
        while (!QFile::exists(directory + "/go")) {
            if (deadline.hasExpired()) return 3;
            QThread::msleep(2);
        }
        const bool ordered = operation == "remove-save" || operation == "save-remove";
        while (ordered && worker == 1 && !QFile::exists(directory + "/0.done")) {
            if (deadline.hasExpired()) return 3;
            QThread::msleep(2);
        }
        if (operation == "places") preferences.rememberPlace("Added " + QString::number(worker), 51 + worker, 21);
        else if (operation == "profiles") {
            if (preferences.createProfile("Added " + QString::number(worker)).isEmpty()) return 4;
        } else if (operation == "edits") preferences.setChartOptions("temperature", {{"title", "Edited " + QString::number(worker)}});
        else if (ordered) {
            if ((operation == "remove-save") == (worker == 0)) preferences.removePlace(50, 20);
            else preferences.savePlace("Renamed", 50, 20);
        } else if (worker == 0) preferences.savePlace("Renamed", 50, 20);
        else preferences.savePlace("New", 51, 21);
        if (ordered && worker == 0) {
            QFile done(directory + "/0.done");
            if (!done.open(QIODevice::WriteOnly)) return 5;
        }
        return 0;
    }
    if (application.arguments().value(1) == "--write-settings") {
        QSettings settings(application.arguments().value(2), QSettings::IniFormat);
        settings.setValue("display/renderer", "cpu");
        settings.setValue("display/windUnit", "km/h");
        settings.setValue("display/utc", true);
        settings.setValue("display/showLegend", true);
        settings.setValue("display/chartHeight", 262);
        settings.setValue("display/profile", "full");
        settings.setValue("places/favorites", QByteArray(R"([{"name":"Kielce","latitude":50.8661,"longitude":20.6286}])"));
        settings.sync();
        return settings.status() == QSettings::NoError ? 0 : 1;
    }
    PreferencesTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_preferences.moc"
