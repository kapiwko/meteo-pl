#include "ProfileEditor.hpp"
#include "SavedPlaces.hpp"
#include "ChartConfiguration.hpp"
#include <QTest>

class PreferencesDomainTest : public QObject {
    Q_OBJECT
private slots:
    void profilesDuplicateEditAndUndoWithoutAnIniFile()
    {
        ProfileEditor editor({},builtinChartProfiles());
        const auto original=editor.chartLayout();
        const auto copy=editor.duplicateProfile("classic");
        QVERIFY(copy.startsWith("user:")); QCOMPARE(editor.profile(),"classic");
        editor.renameProfile("Independent",copy);
        const auto before=editor.profileState(copy);
        const auto result=editor.profileCommand({{"type","removeSeries"},{"chart","temperature"},{"field","dwptmp_point"}},copy);
        QVERIFY(result["ok"].toBool()); QVERIFY(editor.undoProfileCommand(result["undo"].toMap())["ok"].toBool());
        QCOMPARE(editor.profileState(copy),before); QCOMPARE(editor.chartLayout(),original);
        ProfileEditor reopened(editor.state(),builtinChartProfiles());
        QCOMPARE(reopened.chartLayoutForProfile(copy),original);
    }
    void placesPreserveIdentityAndHistoryWithoutSettings()
    {
        SavedPlaces places;
        const auto home=places.editPlace({},"Home",50,20)["place"].toMap();
        const auto id=home["id"].toString();
        QVERIFY(places.editPlace(id,"Moved",51,21)["ok"].toBool());
        QVERIFY(!places.editPlace({},"Duplicate",51,21)["ok"].toBool());
        places.rememberPlace("Moved",51,21);
        QCOMPARE(places.recentPlaces().first().toMap()["id"].toString(),id);
        const auto removed=places.deletePlace(id);
        QVERIFY(places.favorites().isEmpty());
        QVERIFY(places.restorePlace(removed["place"].toMap(),removed["index"].toInt())["ok"].toBool());
        QCOMPARE(places.favorites().first().toMap()["id"].toString(),id);
    }
};
QTEST_GUILESS_MAIN(PreferencesDomainTest)
#include "test_preferences_domain.moc"
