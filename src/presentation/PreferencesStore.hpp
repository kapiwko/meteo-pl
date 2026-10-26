#pragma once
#include "MeteoUiExport.hpp"
#include <QFileSystemWatcher>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <functional>

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
    bool sync();
    bool update(const std::function<void()> &operation, QString *error = nullptr);
    bool updating() const { return m_updating; }
    static QVariantList savedPlacesSnapshot();
  signals:
    void placesChanged();
    void displayChanged();

  private:
    void migrate();
    void publish(QVariantMap values);
    void watchFile();
    QByteArray placesSnapshot() const;
    QByteArray displaySnapshot() const;
    QString m_fileName;
    QVariantMap m_values;
    QVariantMap m_pending;
    bool m_updating = false;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
    QByteArray m_places;
    QByteArray m_display;
};
