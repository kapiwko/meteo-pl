#pragma once
#include "MeteoUiExport.hpp"
#include <QFileSystemWatcher>
#include <QObject>
#include <QSettings>
#include <QTimer>

// Owns the existing INI schema and its cross-process change notifications.
// UserPreferences remains the QML facade for editing profiles and places.
class METEO_UI_EXPORT PreferencesStore : public QObject
{
    Q_OBJECT
public:
    explicit PreferencesStore(QObject *parent = nullptr);
    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    bool contains(const QString &key) const;
    void setValue(const QString &key, const QVariant &value);
    void remove(const QString &key);
    QString fileName() const;
    void sync();
    static QVariantList savedPlacesSnapshot();
signals:
    void placesChanged();
    void displayChanged();

private:
    void migrate();
    QByteArray placesSnapshot() const;
    QByteArray displaySnapshot() const;
    QSettings m_settings;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
    QByteArray m_places;
    QByteArray m_display;
};
