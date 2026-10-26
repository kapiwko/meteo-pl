#pragma once
#include <KRunner/AbstractRunner>
#include <QUrl>

class MeteoRunner final : public KRunner::AbstractRunner {
    Q_OBJECT
public:
    MeteoRunner(QObject *parent, const KPluginMetaData &metadata,
                QUrl endpoint = QUrl(QStringLiteral("https://photon.dev.meteo.pl/api/")));
    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;
private:
    QUrl m_endpoint;
};
