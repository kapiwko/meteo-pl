#include "Localization.hpp"
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QRegularExpression>

static void initializeTranslations()
{
    // libintl needs ordinary files, including in a relocatable bundle. Keep the
    // catalog in resources and extract it once per process, also for Plasma.
    static const QTemporaryDir directory(QDir::tempPath() + QStringLiteral("/meteo-pl-i18n-XXXXXX"));
    static const bool initialized = [] {
        Q_INIT_RESOURCE(meteo_translations);
        if (!directory.isValid()) return false;
        for (const auto &language : QDir(QStringLiteral(":/meteo/locale")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const auto relative = language + QStringLiteral("/LC_MESSAGES/meteo-pl.mo");
            const auto path = directory.path() + '/' + relative;
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile::copy(QStringLiteral(":/meteo/locale/") + relative, path);
        }
        KLocalizedString::addDomainLocaleDir("meteo-pl", directory.path());
        return true;
    }();
    Q_UNUSED(initialized);
}

QString Meteo::i18n(const char *message)
{
    initializeTranslations();
    // Existing presentation code uses QString::arg. Supply literal placeholders
    // to KI18n so it can validate the message without treating them as missing.
    auto translated = ki18nd("meteo-pl", message).relaxSubs();
    static const QRegularExpression placeholder(QStringLiteral("%([1-9][0-9]*)"));
    auto matches = placeholder.globalMatch(QString::fromUtf8(message));
    int last = 0;
    while (matches.hasNext()) last = std::max(last, matches.next().captured(1).toInt());
    for (int i = 1; i <= last; ++i) translated = translated.subs('%' + QString::number(i));
    return translated.toString();
}

QString Meteo::i18np(const char *singular, const char *plural, int count)
{
    initializeTranslations();
    return ki18ndp("meteo-pl", singular, plural).subs(count).toString();
}
