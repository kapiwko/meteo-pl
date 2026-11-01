#include "MeteoRunner.hpp"
#include "RunnerQuery.hpp"
#include "data/ForecastLauncher.hpp"
#include "data/GeocodingClient.hpp"
#include "data/PreferencesFile.hpp"
#include "core/Forecast.hpp"
#include "core/Localization.hpp"
#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>
#include <QEventLoop>
#include <QTimer>

MeteoRunner::MeteoRunner(QObject *parent, const KPluginMetaData &metadata, QUrl endpoint)
    : KRunner::AbstractRunner(parent, metadata), m_endpoint(std::move(endpoint))
{
    setMatchRegex(QRegularExpression(QStringLiteral("^(?:pogoda|weather|meteo)(?:\\s|$)"), QRegularExpression::CaseInsensitiveOption));
    setMinLetterCount(5);
    addSyntax(QStringLiteral("pogoda :q:"), Meteo::i18n("Show a weather forecast for a place"));
    addSyntax(QStringLiteral("weather :q:"), Meteo::i18n("Show a weather forecast for a place"));
    addSyntax(QStringLiteral("meteo"), Meteo::i18n("Open Meteo or choose a saved place"));
}
void MeteoRunner::match(KRunner::RunnerContext &context)
{
    const auto originalQuery = context.query();
    const auto term = Meteo::runnerQuery(originalQuery);
    if (!term || !context.isValid()) return;
    int count = 0;
    const auto addPlace = [&](const QVariantMap &place, bool saved) {
        const auto name = place.value("name").toString();
        const double latitude = place.value("latitude").toDouble(), longitude = place.value("longitude").toDouble();
        if (name.isEmpty() || !Meteo::validLocation(latitude, longitude, "gfs") || count >= 8) return;
        const auto model = Meteo::validLocation(latitude, longitude, "um4_60") ? QStringLiteral("um4_60") : QStringLiteral("gfs");
        KRunner::QueryMatch result(this);
        result.setId(QString::number(latitude, 'f', 6) + ',' + QString::number(longitude, 'f', 6));
        result.setIconName(QStringLiteral("pl.meteo.desktop"));
        result.setText(Meteo::i18n("Forecast for %1").arg(name));
        result.setSubtext(saved ? Meteo::i18n("Saved place") : place.value("description").toString());
        result.setRelevance(saved ? 1.0 : 0.8);
        result.setData(QVariantMap{{"name", name}, {"latitude", latitude}, {"longitude", longitude}, {"model", model}});
        if (context.addMatch(result)) ++count;
    };
    const auto needle = Meteo::searchablePlaceName(*term);
    for (const auto &entry : Meteo::savedPlacesSnapshot()) {
        const auto place = entry.toMap();
        if (Meteo::searchablePlaceName(place.value("name").toString()).contains(needle)) addPlace(place, true);
    }
    if (term->isEmpty()) {
        if (!count) {
            KRunner::QueryMatch result(this);
            result.setId(QStringLiteral("open")); result.setText(Meteo::i18n("Open Meteo"));
            result.setIconName(QStringLiteral("pl.meteo.desktop"));
            context.addMatch(result);
        }
        return;
    }
    if (count || term->size() < 2 || !context.isValid()) return;
    // KRunner calls match in a worker thread. Keep requests local to this query,
    // and abort them as soon as the user replaces it with another query.
    Meteo::GeocodingClient search(nullptr,m_endpoint);
    QVariantList results;
    QEventLoop loop;
    QTimer deadline, cancelled;
    deadline.setSingleShot(true);
    connect(&search,&Meteo::GeocodingClient::received,&loop,[&](const QVariantList &places) { results=places; loop.quit(); });
    connect(&search,&Meteo::GeocodingClient::failed,&loop,&QEventLoop::quit);
    connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&cancelled, &QTimer::timeout, &loop, [&] { if (!context.isValid() || context.query() != originalQuery) loop.quit(); });
    search.search(*term,false);
    deadline.start(2000); cancelled.start(40);
    loop.exec();
    if (context.isValid() && context.query() == originalQuery) for (const auto &entry : results) addPlace(entry.toMap(), false);
    search.cancel();
}
void MeteoRunner::run(const KRunner::RunnerContext &, const KRunner::QueryMatch &match)
{
    const auto place = match.data().toMap();
    Meteo::launchForecast(place.value("name").toString(), place.value("latitude").toDouble(),
                          place.value("longitude").toDouble(), place.value("model").toString());
}
