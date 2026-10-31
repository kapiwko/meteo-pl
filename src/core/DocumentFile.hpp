#pragma once
#include <QFileInfo>
#include <QUrl>

namespace Meteo {
inline bool isContentDocument(const QUrl &url)
{
    return url.isValid() && url.scheme() == QLatin1String("content")
        && !url.host().isEmpty() && !url.path().isEmpty();
}
inline bool isDocumentFile(const QUrl &url)
{
    if (!url.isValid()) return false;
    if (url.isLocalFile()) return !url.toLocalFile().isEmpty();
#ifdef Q_OS_ANDROID
    return isContentDocument(url);
#else
    return false;
#endif
}
inline QString documentPath(const QUrl &url)
{
    // Keep Android document IDs opaque, including encoded colons and slashes.
    return url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::FullyEncoded);
}
inline QString documentDisplayPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : QFileInfo(documentPath(url)).fileName();
}
inline QUrl withDefaultDocumentSuffix(const QUrl &url, const QString &suffix)
{
    // The Android picker has already created the named document. Appending a
    // suffix to its URI would change its ID, rather than the chosen filename.
    if (url.isLocalFile() && QFileInfo(url.toLocalFile()).suffix().isEmpty())
        return QUrl::fromLocalFile(url.toLocalFile() + '.' + suffix);
    return url;
}
}
