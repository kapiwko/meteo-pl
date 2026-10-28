#include "UserPreferences.hpp"
#include "ChartConfiguration.hpp"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
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
    void axisValidationIsAtomicAndProfileExportIncludesUntouchedPresets()
    {
        QTemporaryDir config;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        UserPreferences preferences;
        const auto original = preferences.chartLayout();
        const auto file = QUrl::fromLocalFile(config.filePath("profiles.json"));
        const auto count = preferences.profiles().size();
        QVERIFY(count > 1);
        QCOMPARE(preferences.exportProfiles(file, true)["count"].toInt(), count);
        QVERIFY(!preferences.exportProfilesByIds(file, {})["ok"].toBool());
        QCOMPARE(preferences.exportProfilesByIds(file, {"classic"})["count"].toInt(), 1);
        const auto active = preferences.profile();
        const auto result = preferences.importProfiles(file, false);
        QVERIFY(result["ok"].toBool());
        QCOMPARE(result["ids"].toStringList().size(), 1);
        QCOMPARE(preferences.profile(), active);
        QCOMPARE(preferences.profiles().size(), count + 1);
    }
    void profileEditingIsIndependentAndUndoPreservesOtherChanges()
    {
        QTemporaryDir config;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        UserPreferences preferences;
        preferences.setProfile("classic");
        const auto activeLayout = preferences.chartLayout();
        const auto copy = preferences.duplicateProfile("classic");
        QVERIFY(copy.startsWith("user:"));
        QCOMPARE(preferences.profile(), "classic");
        preferences.renameProfile("Edited separately", copy);
        preferences.setChartOptions("temperature", {{"title", "Edited temperature"}}, copy);
        preferences.setProfileChartHeightForProfile(copy, 250);
        QCOMPARE(preferences.chartLayout(), activeLayout);
        QCOMPARE(preferences.chartHeightForProfile(copy), 250);
        const auto before = preferences.profileState(copy);
        preferences.removeSeries("temperature", "dwptmp_point", copy);
        const auto after = preferences.profileState(copy);
        preferences.setChartOptions("temperature", {{"title", "A concurrent change"}}, "classic");
        QVERIFY(preferences.restoreProfileState(before, after)["ok"].toBool());
        QCOMPARE(preferences.profileState(copy), before);
        QCOMPARE(preferences.chartLayout().first().toMap()["title"].toString(), "A concurrent change");
        preferences.removeSeries("temperature", "dwptmp_point", copy);
        const auto removed = preferences.profileState(copy);
        preferences.renameProfile("Changed after deletion", copy);
        const auto changed = preferences.profileState(copy);
        QVERIFY(!preferences.restoreProfileState(before, removed)["ok"].toBool());
        QCOMPARE(preferences.profileState(copy), changed);
        const auto beforeDelete = preferences.profileState(copy);
        preferences.deleteProfile(copy);
        QVERIFY(preferences.restoreProfileState(beforeDelete, preferences.profileState(copy))["ok"].toBool());
        QCOMPARE(preferences.profileState(copy), beforeDelete);
        QCOMPARE(preferences.profile(), "classic");
        const auto presetBefore = preferences.profileState("classic");
        preferences.resetProfile("classic");
        QVERIFY(preferences.restoreProfileState(presetBefore, preferences.profileState("classic"))["ok"].toBool());
        QCOMPARE(preferences.chartLayout().first().toMap()["title"].toString(), "A concurrent change");
    }
    void legacyPlaceKeysRemainStableAfterMovingThePoint()
    {
        QTemporaryDir config;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "meteo-pl", "desktop");
        settings.setValue("places/listVersion", 1);
        settings.setValue("places/favorites", QByteArray(R"([{"name":"Home","latitude":50,"longitude":20}])"));
        settings.sync();
        UserPreferences preferences;
        const auto id = preferences.favorites().first().toMap()["id"].toString();
        QCOMPARE(id, "50.000000,20.000000");
        QVERIFY(preferences.editPlace(id, "Home", 51, 21)["ok"].toBool());
        UserPreferences reopened;
        QCOMPARE(reopened.favorites().first().toMap()["id"].toString(), id);
    }
    void placeEditingKeepsIdentityAndDeletionCanBeUndone()
    {
        QTemporaryDir config;
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        UserPreferences preferences;
        const auto added = preferences.editPlace({}, "Home", 50, 20);
        QVERIFY(added["ok"].toBool());
        const auto id = added["place"].toMap()["id"].toString();
        QVERIFY(!id.isEmpty());
        preferences.savePlace("Other", 51, 21);
        preferences.rememberPlace("Stale name", 50, 20);
        QCOMPARE(preferences.placeAt(50, 20)["name"].toString(), "Home");
        QVERIFY(preferences.editPlace(id, "Moved home", 52, 22)["ok"].toBool());
        QCOMPARE(preferences.favorites().size(), 2);
        QCOMPARE(preferences.favorites().first().toMap()["id"].toString(), id);
        QVERIFY(preferences.placeAt(50, 20).isEmpty());
        const auto before = preferences.favorites();
        QVERIFY(!preferences.editPlace(id, "Collision", 51, 21)["ok"].toBool());
        QCOMPARE(preferences.favorites(), before);
        const auto removed = preferences.deletePlace(id);
        QVERIFY(removed["ok"].toBool());
        QVERIFY(!preferences.editPlace(id, "Stale editor", 52, 22)["ok"].toBool());
        QVERIFY(preferences.restorePlace(removed["place"].toMap(), removed["index"].toInt())["ok"].toBool());
        QCOMPARE(preferences.favorites(), before);
        UserPreferences reopened;
        QCOMPARE(reopened.favorites(), before);
    }
    void persistenceDoesNotRequireAWritableSystemTempDirectory()
    {
#ifndef Q_OS_UNIX
        QSKIP("This exercises the Unix TMPDIR used by Android.");
#endif
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QFile blocked(config.filePath("not-a-directory"));
        QVERIFY(blocked.open(QIODevice::WriteOnly));
        blocked.close();
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("TMPDIR", blocked.fileName());
        environment.insert("XDG_CACHE_HOME", config.filePath("cache"));
        environment.insert("QT_FORCE_STDERR_LOGGING", "1");
        QProcess worker;
        worker.setProcessEnvironment(environment);
        worker.start(QCoreApplication::applicationFilePath(), {"--check-private-temporaries", config.path()});
        QVERIFY(worker.waitForFinished(10000));
        QCOMPARE(worker.exitStatus(), QProcess::NormalExit);
        const auto errors = worker.readAllStandardError();
        QVERIFY2(worker.exitCode() == 0, errors.constData());
    }
    void legacyCleanupPreservesEditedDisabledChartsAndOnlyRunsOnce()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
        auto layout = defaultChartCatalog();
        QStringList enabled;
        for (const auto &entry : defaultChartLayout("classic")) enabled.append(entry.toMap()["key"].toString());
        for (auto &value : layout) {
            auto chart = value.toMap();
            chart["enabled"] = enabled.contains(chart["key"].toString());
            if (chart["key"] == "ground") chart["title"] = "My hidden ground chart";
            if (chart["key"] == "humidity") chart["height"] = 220;
            value = chart;
        }
        const auto before = normalizeChartLayout(layout);
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "meteo-pl", "desktop");
        settings.setValue("display/layoutVersion", 3);
        settings.setValue("display/profile", "user:old");
        settings.setValue("display/customLayout", QJsonDocument::fromVariant(layout).toJson());
        settings.setValue("display/savedProfiles", QJsonDocument::fromVariant(QVariantList{
            QVariantMap{{"id", "user:old"}, {"name", "Old profile"}, {"charts", layout}}}).toJson());
        settings.sync();
        UserPreferences preferences;
        const auto cleaned = preferences.chartLayout();
        QCOMPARE(cleaned.size(), enabled.size() + 2);
        for (const auto &entry : cleaned) {
            const auto chart = entry.toMap();
            QVERIFY(chart["enabled"].toBool() || chart["key"] == "ground" || chart["key"] == "humidity");
            QVERIFY(before.contains(entry));
        }
        QCOMPARE(preferences.chartLayoutForProfile("custom"), cleaned);
        preferences.setChartEnabled("temperature", false);
        UserPreferences reopened;
        QCOMPARE(reopened.chartLayout().size(), cleaned.size());
        QVERIFY(!reopened.chartLayout().first().toMap()["enabled"].toBool());
    }
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
            QCOMPARE(persisted.profiles().size(), builtinChartProfiles().size() + 4);
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
        QCOMPARE(bindings->property("profile").toString(), "basic");
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
    if (application.arguments().value(1) == "--check-private-temporaries") {
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, application.arguments().value(2));
        PreferencesStore store;
        QString error;
        if (!store.update([&] { store.setValue("display/profile", "basic"); }, &error)) {
            qWarning().noquote() << error;
            return 2;
        }
        PreferencesStore reopened;
        return reopened.value("display/profile").toString() == "basic" ? 0 : 3;
    }
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
        settings.setValue("display/profile", "basic");
        settings.setValue("places/favorites", QByteArray(R"([{"name":"Kielce","latitude":50.8661,"longitude":20.6286}])"));
        settings.sync();
        return settings.status() == QSettings::NoError ? 0 : 1;
    }
    PreferencesTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_preferences.moc"
