#include "core/Localization.hpp"
#include "core/DocumentFile.hpp"
#include "ExportNotifications.hpp"
#include <KNotification>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>

void ExportNotifications::saved(const QUrl &file, const QString &title)
{
    if (!Meteo::isDocumentFile(file) || !QFileInfo::exists(Meteo::documentPath(file))) return;
    // Headless renders and automated export tests must not post desktop popups.
    if (QGuiApplication::platformName() == QLatin1String("offscreen") || QGuiApplication::platformName() == QLatin1String("minimal")) return;
    auto *notification = new KNotification(QStringLiteral("exportFinished"), KNotification::CloseOnTimeout, this);
    notification->setComponentName(QStringLiteral("pl.meteo.desktop"));
    notification->setIconName(QStringLiteral("pl.meteo.desktop"));
    notification->setTitle(title);
    notification->setText(QFileInfo(Meteo::documentPath(file)).fileName().toHtmlEscaped());
    notification->setUrls({file});
    notification->setHint(QStringLiteral("desktop-entry"), QStringLiteral("pl.meteo.desktop"));
    const auto open = [file] { QDesktopServices::openUrl(file); };
    connect(notification->addDefaultAction(Meteo::i18n("Open")), &KNotificationAction::activated, notification, open);
    connect(notification->addAction(Meteo::i18n("Open")), &KNotificationAction::activated, notification, open);
    if (file.isLocalFile()) connect(notification->addAction(Meteo::i18n("Open folder")), &KNotificationAction::activated, notification, [file] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file.toLocalFile()).absolutePath()));
    });
    notification->sendEvent();
}
bool ExportNotifications::openFile(const QUrl &file) const { return Meteo::isDocumentFile(file) && QDesktopServices::openUrl(file); }
bool ExportNotifications::openFolder(const QUrl &file) const
{
    return file.isLocalFile() && QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file.toLocalFile()).absolutePath()));
}
